#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QUdpSocket>
#include "core/Constants.h"

// UDP audio channel (spec §4.2): one socket, both directions, one VCAM frame
// per datagram. Opus packets ride here — they fit a single datagram and a
// lost one is 10-20 ms the jitter buffer rides out — so audio can never stall
// behind a video frame or a TCP retransmit (head-of-line blocking). PCM stays
// on the TCP stream. The phone advertises its port in HELLO "audioPort"; we
// advertise ours in the CONTROL snapshot; datagrams from anyone but the
// connected peer are dropped, and the header timestamp doubles as a sequence
// number for reorder/dup rejection.
class AudioChannel : public QObject {
    Q_OBJECT

public:
    explicit AudioChannel(QObject *parent = nullptr);

    // Our bound port (for the CONTROL snapshot); 0 if bind failed.
    int localPort() const { return m_socket.localPort(); }

    // The connected phone's address + HELLO audioPort. Resets sequencing.
    void setPeer(const QHostAddress &address, int port);
    void clearPeer();
    bool hasPeer() const { return m_peerPort > 0; }

    // One audio frame -> one datagram. Returns false when the frame doesn't
    // fit or there is no peer — the caller falls back to TCP.
    bool sendFrame(const QByteArray &payload, int sampleRate, int channels,
                   vc::FrameFormat format);

signals:
    // Phone mic frame received via UDP (already peer-filtered + de-duped).
    void frameReceived(const QByteArray &payload, int sampleRate, int channels,
                       vc::FrameFormat format);

private:
    void onReadyRead();

    QUdpSocket m_socket;
    QHostAddress m_peerAddress;
    int m_peerPort = 0;
    quint64 m_lastRxTs = 0;
};
