#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QByteArray>
#include <QJsonObject>
#include "core/Constants.h"
#include "core/FrameData.h"

// TCP client for the phone's frame server (see CONNECTIVITY_PROTOCOL.md).
// Parses the shared 24-byte header and dispatches HELLO / HEARTBEAT / VIDEO.
class StreamReceiver : public QObject {
    Q_OBJECT

public:
    explicit StreamReceiver(QObject *parent = nullptr);
    ~StreamReceiver();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;

    // Desktop -> Phone CONTROL frame (format=4/type=1, JSON body). The socket
    // is bidirectional: we keep reading VIDEO/STATUS while writing these.
    void sendControl(const QJsonObject &patch);

signals:
    void frameReceived(const FrameData &frame);          // VIDEO
    void helloReceived(const QString &name, const QString &os,
                       int maxW, int maxH,
                       int battery, bool charging,
                       const QString &lens);             // HELLO (once on accept)
    void statusReceived(int battery, bool charging);     // STATUS (periodic, JSON)
    // Active-camera descriptor (e.g. "Back · ƒ1.8") from HELLO and re-sent in
    // STATUS whenever the phone flips lenses. Empty => unknown.
    void lensReceived(const QString &lens);
    // Applied camera-control state echoed by the phone in STATUS controls{}.
    void controlStateReceived(const QJsonObject &controls);
    void heartbeatReceived();                            // HEARTBEAT (zero-length)
    void connected();                                    // TCP socket up
    void disconnected();
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    bool parseFrame();
    void dispatchHello(const QByteArray &payload);
    void dispatchStatus(const QByteArray &payload);

    QTcpSocket *m_socket;
    QByteArray  m_buffer;
    QTimer      m_connectTimer;   // aborts after 10 s if TCP handshake stalls

    static constexpr int HEADER_SIZE       = vc::kFrameHeaderSize;
    static constexpr int CONNECT_TIMEOUT_MS = 10000;
};
