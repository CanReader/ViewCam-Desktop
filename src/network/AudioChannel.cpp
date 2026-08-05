#include "network/AudioChannel.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QtEndian>
#include <cstring>

namespace {
// Below typical MTU (1500) minus IP/UDP headers, with margin: one frame must
// never fragment — a lost fragment loses the datagram anyway, twice as often.
constexpr int kMaxDatagramPayload = 1400;
} // namespace

AudioChannel::AudioChannel(QObject *parent) : QObject(parent) {
    if (!m_socket.bind()) // ephemeral port, all interfaces
        VC_WARN("Audio UDP bind failed — audio stays on TCP");
    else
        VC_INFO("Audio UDP channel on port {}", m_socket.localPort());
    connect(&m_socket, &QUdpSocket::readyRead, this, &AudioChannel::onReadyRead);
}

void AudioChannel::setPeer(const QHostAddress &address, int port) {
    m_peerAddress = address;
    m_peerPort = port;
    m_lastRxTs = 0;
    VC_INFO("Audio UDP peer: {}:{}", address.toString().toStdString(), port);
}

void AudioChannel::clearPeer() {
    m_peerAddress = QHostAddress();
    m_peerPort = 0;
    m_lastRxTs = 0;
}

bool AudioChannel::sendFrame(const QByteArray &payload, int sampleRate,
                             int channels, vc::FrameFormat format) {
    if (m_peerPort <= 0 || m_socket.localPort() == 0) return false;
    if (payload.size() + vc::kFrameHeaderSize > kMaxDatagramPayload) return false;

    QByteArray dgram(vc::kFrameHeaderSize, 0);
    auto *p = reinterpret_cast<uchar *>(dgram.data());
    std::memcpy(p + vc::hdr::kMagic, vc::kFrameMagic, 4);
    qToLittleEndian<quint32>(quint32(payload.size()), p + vc::hdr::kPayloadLen);
    qToLittleEndian<quint64>(
        quint64(QDateTime::currentMSecsSinceEpoch()) * 1000,
        p + vc::hdr::kTimestamp);
    qToLittleEndian<quint16>(quint16(sampleRate), p + vc::hdr::kWidth);
    qToLittleEndian<quint16>(quint16(channels), p + vc::hdr::kHeight);
    p[vc::hdr::kFormat] = uchar(format);
    p[vc::hdr::kType] = uchar(vc::FrameType::Audio);
    dgram.append(payload);

    return m_socket.writeDatagram(dgram, m_peerAddress, quint16(m_peerPort)) ==
           dgram.size();
}

void AudioChannel::onReadyRead() {
    while (m_socket.hasPendingDatagrams()) {
        QByteArray dgram;
        dgram.resize(int(m_socket.pendingDatagramSize()));
        QHostAddress sender;
        m_socket.readDatagram(dgram.data(), dgram.size(), &sender);

        // Only the connected phone may feed the microphone.
        if (m_peerPort <= 0 || !sender.isEqual(m_peerAddress,
                                               QHostAddress::TolerantConversion))
            continue;
        if (dgram.size() < vc::kFrameHeaderSize ||
            std::memcmp(dgram.constData(), vc::kFrameMagic, 4) != 0)
            continue;

        const auto *p = reinterpret_cast<const uchar *>(dgram.constData());
        const quint32 len = qFromLittleEndian<quint32>(p + vc::hdr::kPayloadLen);
        const quint64 ts = qFromLittleEndian<quint64>(p + vc::hdr::kTimestamp);
        const quint16 rate = qFromLittleEndian<quint16>(p + vc::hdr::kWidth);
        const quint16 channels = qFromLittleEndian<quint16>(p + vc::hdr::kHeight);
        const auto format = vc::FrameFormat(p[vc::hdr::kFormat]);

        if (int(len) != dgram.size() - vc::kFrameHeaderSize) continue;
        if (format != vc::FrameFormat::AudioPcm &&
            format != vc::FrameFormat::AudioOpus)
            continue;
        if (ts <= m_lastRxTs) continue; // reordered/duplicated datagram
        m_lastRxTs = ts;
        if (rate == 0 || channels == 0 || len == 0) continue;

        emit frameReceived(dgram.mid(vc::kFrameHeaderSize), rate, channels,
                           format);
    }
}
