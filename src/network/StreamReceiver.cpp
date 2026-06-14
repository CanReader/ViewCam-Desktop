#include "network/StreamReceiver.h"
#include "core/Logger.h"
#include <QtEndian>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstring>

namespace {
// Build a 24-byte little-endian frame header (see CONNECTIVITY_PROTOCOL.md §4).
QByteArray makeHeader(quint32 payloadLen, quint16 w, quint16 h,
                      vc::FrameFormat fmt, vc::FrameType type) {
    QByteArray hdr(vc::kFrameHeaderSize, 0);
    auto *p = reinterpret_cast<uchar *>(hdr.data());
    std::memcpy(p + vc::hdr::kMagic, vc::kFrameMagic, 4);
    qToLittleEndian<quint32>(payloadLen, p + vc::hdr::kPayloadLen);
    qToLittleEndian<quint64>(
        static_cast<quint64>(QDateTime::currentMSecsSinceEpoch()) * 1000,
        p + vc::hdr::kTimestamp);
    qToLittleEndian<quint16>(w, p + vc::hdr::kWidth);
    qToLittleEndian<quint16>(h, p + vc::hdr::kHeight);
    p[vc::hdr::kFormat] = static_cast<uchar>(fmt);
    p[vc::hdr::kType] = static_cast<uchar>(type);
    return hdr;
}
} // namespace

StreamReceiver::StreamReceiver(QObject *parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead, this, &StreamReceiver::onReadyRead);
    connect(m_socket, &QTcpSocket::connected, this, &StreamReceiver::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &StreamReceiver::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &StreamReceiver::onError);
    VC_DEBUG("StreamReceiver created");
}

StreamReceiver::~StreamReceiver() {
    disconnect();
}

void StreamReceiver::connectToHost(const QString &host, int port) {
    VC_INFO("StreamReceiver connecting to {}:{}", host.toStdString(), port);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        VC_DEBUG("Aborting previous connection");
        m_socket->abort();
    }
    m_buffer.clear();
    m_socket->connectToHost(host, port);
}

void StreamReceiver::disconnect() {
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        VC_DEBUG("Disconnecting socket");
        m_socket->abort();
    }
    m_buffer.clear();
}

bool StreamReceiver::isConnected() const {
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void StreamReceiver::sendControl(const QJsonObject &patch) {
    if (!isConnected()) {
        VC_DEBUG("sendControl ignored: socket not connected");
        return;
    }
    const QByteArray body = QJsonDocument(patch).toJson(QJsonDocument::Compact);
    m_socket->write(makeHeader(static_cast<quint32>(body.size()), 0, 0,
                               vc::FrameFormat::Control, vc::FrameType::Control));
    m_socket->write(body);
    VC_INFO("CONTROL -> phone: {}", body.constData());
}

void StreamReceiver::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    while (parseFrame()) {}
}

bool StreamReceiver::parseFrame() {
    if (m_buffer.size() < HEADER_SIZE) return false;

    // Verify magic at the head; otherwise resync to the next "VCAM".
    if (m_buffer[0] != 'V' || m_buffer[1] != 'C' ||
        m_buffer[2] != 'A' || m_buffer[3] != 'M') {
        const int idx = m_buffer.indexOf(vc::kFrameMagic, 1);
        if (idx < 0) {
            VC_WARN("Lost sync, no VCAM magic, discarding {} bytes", m_buffer.size());
            m_buffer.clear();
        } else {
            VC_WARN("Lost sync, skipping {} bytes to resync", idx);
            m_buffer.remove(0, idx);
        }
        return false;
    }

    // Parse header (little-endian, see CONNECTIVITY_PROTOCOL.md §4)
    const auto *data = reinterpret_cast<const uchar *>(m_buffer.constData());
    const uint32_t payloadLen = qFromLittleEndian<uint32_t>(data + vc::hdr::kPayloadLen);
    const uint64_t timestamp  = qFromLittleEndian<uint64_t>(data + vc::hdr::kTimestamp);
    const uint16_t width      = qFromLittleEndian<uint16_t>(data + vc::hdr::kWidth);
    const uint16_t height     = qFromLittleEndian<uint16_t>(data + vc::hdr::kHeight);
    const auto format = static_cast<vc::FrameFormat>(data[vc::hdr::kFormat]);

    if (payloadLen > static_cast<uint32_t>(vc::kMaxFrameBytes)) {
        VC_ERROR("payloadLen {} exceeds {} byte cap, resyncing",
                 payloadLen, vc::kMaxFrameBytes);
        m_buffer.remove(0, 4); // skip this magic, hunt for the next
        return true;
    }

    const int totalSize = HEADER_SIZE + static_cast<int>(payloadLen);
    if (m_buffer.size() < totalSize) return false; // wait for full payload

    const QByteArray payload = m_buffer.mid(HEADER_SIZE, payloadLen);
    m_buffer.remove(0, totalSize);

    switch (format) {
    case vc::FrameFormat::Hello:
        VC_INFO("HELLO frame ({} bytes)", payloadLen);
        dispatchHello(payload);
        break;
    case vc::FrameFormat::Heartbeat:
        // format=3/type=1 is overloaded: a JSON body => STATUS (live battery
        // updates), a zero-length frame => plain keep-alive heartbeat.
        if (payloadLen > 0) {
            VC_TRACE("STATUS frame ({} bytes)", payloadLen);
            dispatchStatus(payload);
        } else {
            VC_TRACE("HEARTBEAT");
            emit heartbeatReceived();
        }
        break;
    case vc::FrameFormat::Mjpeg: {
        FrameData frame;
        frame.jpegData = payload;
        frame.width = width;
        frame.height = height;
        frame.timestamp = timestamp;
        VC_TRACE("VIDEO frame {}x{}, {} bytes", width, height, payloadLen);
        emit frameReceived(frame);
        break;
    }
    case vc::FrameFormat::H264:
        VC_WARN("H264 frame received but not supported (MJPEG only); dropping");
        break;
    }
    return true;
}

namespace {
// Battery is optional; absent or out-of-range => -1 (unknown). Both sides
// agree -1 is the sentinel, and the desktop never renders it raw (it shows "—").
int parseBattery(const QJsonObject &o) {
    if (!o.contains("battery"))
        return -1;
    const int b = o.value("battery").toInt(-1);
    return (b < 0 || b > 100) ? -1 : b;
}
} // namespace

void StreamReceiver::dispatchHello(const QByteArray &payload) {
    // {"name":..,"os":..,"maxW":..,"maxH":..,"deviceId":..,
    //  "battery":<0-100|-1>,"charging":<bool>}
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        VC_WARN("HELLO payload is not valid JSON");
        emit helloReceived(QString(), QString(), 0, 0, -1, false, QString());
        return;
    }
    const QJsonObject o = doc.object();
    const QString name = o.value("name").toString();
    const QString os = o.value("os").toString();
    const int maxW = o.value("maxW").toInt();
    const int maxH = o.value("maxH").toInt();
    const int battery = parseBattery(o);
    const bool charging = o.value("charging").toBool(false);
    const QString lens = o.value("lens").toString();
    VC_INFO("HELLO from '{}' ({}), caps {}x{}, battery {}, charging {}, lens '{}'",
            name.toStdString(), os.toStdString(), maxW, maxH, battery, charging,
            lens.toStdString());
    emit helloReceived(name, os, maxW, maxH, battery, charging, lens);
}

void StreamReceiver::dispatchStatus(const QByteArray &payload) {
    // STATUS body: {"battery":<0-100>,"charging":<bool>} — periodic live update.
    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        VC_WARN("STATUS payload is not valid JSON");
        return;
    }
    const QJsonObject o = doc.object();
    const int battery = parseBattery(o);
    const bool charging = o.value("charging").toBool(false);
    VC_TRACE("STATUS battery {}, charging {}", battery, charging);
    emit statusReceived(battery, charging);

    // Optional lens descriptor — re-sent when the phone flips lenses.
    if (o.contains("lens")) {
        const QString lens = o.value("lens").toString();
        if (!lens.isEmpty())
            emit lensReceived(lens);
    }

    // Optional controls{} echo: the phone reports what it actually applied
    // (and what it can't, e.g. hdrSupported=false) so the UI can reflect it.
    if (o.value("controls").isObject())
        emit controlStateReceived(o.value("controls").toObject());
}

void StreamReceiver::onConnected() {
    VC_INFO("TCP connection established to {}:{}",
            m_socket->peerAddress().toString().toStdString(),
            m_socket->peerPort());
    emit connected();
}

void StreamReceiver::onDisconnected() {
    VC_INFO("TCP connection closed");
    m_buffer.clear();
    emit disconnected();
}

void StreamReceiver::onError(QAbstractSocket::SocketError error) {
    VC_ERROR("Socket error ({}): {}", static_cast<int>(error),
             m_socket->errorString().toStdString());
    emit errorOccurred(m_socket->errorString());
}
