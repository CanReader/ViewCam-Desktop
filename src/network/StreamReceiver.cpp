#include "network/StreamReceiver.h"
#include "core/Logger.h"
#include <QtEndian>
#include <QJsonDocument>
#include <QJsonObject>

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

void StreamReceiver::onReadyRead() {
    m_buffer.append(m_socket->readAll());
    while (parseFrame()) {}
}

bool StreamReceiver::parseFrame() {
    if (m_buffer.size() < HEADER_SIZE) return false;

    // Verify magic
    if (m_buffer[0] != 'V' || m_buffer[1] != 'C' ||
        m_buffer[2] != 'A' || m_buffer[3] != 'M') {
        // Lost sync - try to find next VCAM magic
        int idx = m_buffer.indexOf("VCAM", 1);
        if (idx < 0) {
            VC_WARN("Lost sync, no VCAM magic found, discarding {} bytes", m_buffer.size());
            m_buffer.clear();
        } else {
            VC_WARN("Lost sync, skipping {} bytes to resync", idx);
            m_buffer.remove(0, idx);
        }
        return false;
    }

    // Parse header (little-endian)
    auto *data = reinterpret_cast<const uchar *>(m_buffer.constData());

    uint32_t frameSize = qFromLittleEndian<uint32_t>(data + 4);
    uint64_t timestamp = qFromLittleEndian<uint64_t>(data + 8);
    uint16_t width = qFromLittleEndian<uint16_t>(data + 16);
    uint16_t height = qFromLittleEndian<uint16_t>(data + 18);
    uint8_t format = data[20];
    // bytes 21-23: reserved

    if (frameSize > 10 * 1024 * 1024) {
        VC_ERROR("Frame size {} exceeds 10MB, likely corrupted, skipping", frameSize);
        m_buffer.remove(0, 4);
        return true; // try again
    }

    int totalSize = HEADER_SIZE + static_cast<int>(frameSize);
    if (m_buffer.size() < totalSize) return false;

    // Handle control frames (format 0xFE)
    if (format == 0xFE) {
        QByteArray payload = m_buffer.mid(HEADER_SIZE, frameSize);
        m_buffer.remove(0, totalSize);

        QJsonDocument doc = QJsonDocument::fromJson(payload);
        if (doc.isObject()) {
            QString reason = doc.object().value("reason").toString();
            VC_INFO("Control frame received: reason={}", reason.toStdString());
            if (reason == "session_limit") {
                emit sessionLimitReached();
            }
        }
        return true;
    }

    FrameData frame;
    frame.jpegData = m_buffer.mid(HEADER_SIZE, frameSize);
    frame.width = width;
    frame.height = height;
    frame.timestamp = timestamp;

    m_buffer.remove(0, totalSize);

    VC_TRACE("Frame received: {}x{}, {} bytes", width, height, frameSize);
    emit frameReceived(frame);
    return true;
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
