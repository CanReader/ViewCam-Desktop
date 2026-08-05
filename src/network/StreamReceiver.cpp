#include "network/StreamReceiver.h"
#include "core/Logger.h"
#include <QtEndian>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>
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

    // The receiver lives on a dedicated network thread (moveToThread in
    // AppController). A plain member QObject does NOT move with its owner —
    // only children do — so parent the timer or its start()/stop() would run
    // against the wrong thread.
    m_connectTimer.setParent(this);
    m_connectTimer.setSingleShot(true);
    m_connectTimer.setInterval(CONNECT_TIMEOUT_MS);
    connect(&m_connectTimer, &QTimer::timeout, this, [this]() {
        VC_WARN("TCP connect timeout after {}ms — aborting", CONNECT_TIMEOUT_MS);
        m_socket->abort();
    });
    VC_DEBUG("StreamReceiver created");
}

StreamReceiver::~StreamReceiver() {
    // abort() on a connected socket emits disconnected() SYNCHRONOUSLY, and our
    // signal connections are still live here (QObject severs them only after
    // this dtor body). AppController destroys members in reverse declaration
    // order, so its disconnected-lambda would touch an already-freed
    // ConnectionViewModel — block the socket's signals during teardown.
    // Deliberately NOT the marshaling disconnect(): by the time this dtor runs
    // the network thread has been stopped (AppController's dtor blockingly
    // aborted the socket there first), so a queued self-invoke would never run.
    m_socket->blockSignals(true);
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
}

void StreamReceiver::connectToHost(const QString &host, int port) {
    // Public API is called from the GUI thread; the socket lives on the
    // network thread — self-marshal so all socket access stays single-threaded.
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this, [this, host, port] { connectToHost(host, port); },
            Qt::QueuedConnection);
        return;
    }
    VC_INFO("StreamReceiver connecting to {}:{}", host.toStdString(), port);
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        VC_DEBUG("Aborting previous connection");
        m_socket->abort();
    }
    m_buffer.clear();
    m_socket->connectToHost(host, port);
    m_connectTimer.start();
}

void StreamReceiver::disconnect() {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this] { disconnect(); },
                                  Qt::QueuedConnection);
        return;
    }
    m_connectTimer.stop();
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
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, patch] { sendControl(patch); },
                                  Qt::QueuedConnection);
        return;
    }
    if (!isConnected()) {
        VC_DEBUG("sendControl ignored: socket not connected");
        return;
    }
    const QByteArray body = QJsonDocument(patch).toJson(QJsonDocument::Compact);
    const QByteArray header = makeHeader(static_cast<quint32>(body.size()), 0, 0,
                                         vc::FrameFormat::Control, vc::FrameType::Control);
    if (m_socket->write(header) != header.size() ||
        m_socket->write(body)   != body.size()) {
        VC_ERROR("sendControl write failed: {}", m_socket->errorString().toStdString());
        emit errorOccurred(m_socket->errorString());
        return;
    }
    VC_INFO("CONTROL -> phone: {}", body.constData());
}

void StreamReceiver::sendAudio(const QByteArray &payload, int sampleRate,
                               int channels, vc::FrameFormat format) {
    // Called from the GUI thread (system-audio chunkReady handler) at ~50
    // chunks/s; the socket lives on the network thread. An unmarshaled write
    // would interleave with the net thread's own control writes and corrupt
    // the outgoing frame stream — same self-marshal as sendControl().
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(
            this,
            [this, payload, sampleRate, channels, format] {
                sendAudio(payload, sampleRate, channels, format);
            },
            Qt::QueuedConnection);
        return;
    }
    if (!isConnected() || payload.isEmpty()) return;
    const QByteArray header = makeHeader(static_cast<quint32>(payload.size()),
                                         static_cast<quint16>(sampleRate),
                                         static_cast<quint16>(channels),
                                         format, vc::FrameType::Audio);
    if (m_socket->write(header)  != header.size() ||
        m_socket->write(payload) != payload.size()) {
        VC_ERROR("sendAudio write failed: {}", m_socket->errorString().toStdString());
        emit errorOccurred(m_socket->errorString());
    }
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
            // Keep a trailing partial magic ("V"/"VC"/"VCA") — the next frame's
            // magic may straddle the read boundary, and clearing it would make
            // that frame unrecoverable, prolonging the desync by a full frame.
            int keep = 0;
            for (int k = 3; k >= 1; --k) {
                if (m_buffer.size() >= k &&
                    m_buffer.right(k) == QByteArray(vc::kFrameMagic, k)) {
                    keep = k;
                    break;
                }
            }
            VC_WARN("Lost sync, no VCAM magic, discarding {} bytes (keeping {})",
                    m_buffer.size() - keep, keep);
            m_buffer.remove(0, m_buffer.size() - keep);
            return false; // nothing left to parse
        } else {
            VC_WARN("Lost sync, skipping {} bytes to resync", idx);
            m_buffer.remove(0, idx);
            return true;  // re-enter to attempt parse at new position
        }
    }

    // Parse header (little-endian, see CONNECTIVITY_PROTOCOL.md §4)
    const auto *data = reinterpret_cast<const uchar *>(m_buffer.constData());
    const uint32_t payloadLen = qFromLittleEndian<uint32_t>(data + vc::hdr::kPayloadLen);
    const uint64_t timestamp  = qFromLittleEndian<uint64_t>(data + vc::hdr::kTimestamp);
    const uint16_t width      = qFromLittleEndian<uint16_t>(data + vc::hdr::kWidth);
    const uint16_t height     = qFromLittleEndian<uint16_t>(data + vc::hdr::kHeight);
    const auto format = static_cast<vc::FrameFormat>(data[vc::hdr::kFormat]);
    // Byte 22 (spec §4): sensor-to-upright transform the phone no longer
    // applies itself. Zero on frames from older phone builds, which therefore
    // decode exactly as before. Read here — `data` dangles after the remove().
    const uint8_t orient = data[vc::hdr::kOrient];

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
    case vc::FrameFormat::Mjpeg:
    case vc::FrameFormat::H264: {
        FrameData frame;
        frame.jpegData = payload;
        frame.format = static_cast<uint8_t>(format);
        frame.width = width;
        frame.height = height;
        frame.timestamp = timestamp;
        frame.rotationDegrees = (orient & 0x03) * 90;
        frame.mirror = (orient & 0x08) != 0;
        VC_TRACE("VIDEO frame fmt={} {}x{}, {} bytes, rot={} mirror={}",
                 static_cast<int>(format), width, height, payloadLen,
                 frame.rotationDegrees, frame.mirror);
        emit frameReceived(frame);
        break;
    }
    case vc::FrameFormat::AudioPcm:
    case vc::FrameFormat::AudioOpus:
        // Spec §4.1: width carries the sample rate, height the channel count.
        VC_TRACE("AUDIO frame fmt={} {} Hz x{}, {} bytes",
                 static_cast<int>(format), width, height, payloadLen);
        if (width > 0 && height > 0 && payloadLen > 0)
            emit audioReceived(payload, width, height, format);
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
        emit helloReceived(QString(), QString(), 0, 0, -1, false, QString(), false);
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
    const bool pro = o.value("pro").toBool(false); // absent on older phones => free
    VC_INFO("HELLO from '{}' ({}), caps {}x{}, battery {}, charging {}, pro {}, lens '{}'",
            name.toStdString(), os.toStdString(), maxW, maxH, battery, charging,
            pro, lens.toStdString());
    // Before helloReceived: AppController folds audio keys into the control
    // snapshot it sends from its helloReceived handler. Absent ⇒ video-only.
    emit audioCapableReceived(o.value("audio").toBool(false));
    // UDP audio channel port (spec §4.2, phones ≥2.1). 0/absent ⇒ TCP only.
    emit audioPortReceived(o.value("audioPort").toInt(0));
    emit helloReceived(name, os, maxW, maxH, battery, charging, lens, pro);
    emit proReceived(pro);

    // Encoder codecs the phone can produce ("codecs":["mjpeg","h264"], phones
    // ≥1.2.0). Absent on older phones / iOS ⇒ MJPEG only — the Settings
    // protocol picker grays out what the phone can't do.
    QStringList codecs{QStringLiteral("mjpeg")};
    for (const auto &v : o.value("codecs").toArray()) {
        const QString c = v.toString().toLower();
        if (!c.isEmpty() && !codecs.contains(c)) codecs.append(c);
    }
    emit phoneCodecsReceived(codecs);
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

    // Live entitlement: a mid-session purchase flips this so the watermark drops
    // and 4K unlocks without a reconnect. Absent on older phones => leave as-is.
    if (o.contains("pro"))
        emit proReceived(o.value("pro").toBool(false));

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
    m_connectTimer.stop();
    // TCP_NODELAY: CONTROL frames (zoom, torch, focus) and 20 ms speaker AUDIO
    // chunks are tiny — never let Nagle hold them back behind an unacked
    // segment. SO_KEEPALIVE: detect a silently dead link (AP roam, phone
    // sleep) at the OS level too, not just via the app-level watchdog.
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
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
