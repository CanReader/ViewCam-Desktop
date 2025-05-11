#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QByteArray>
#include "core/FrameData.h"

class StreamReceiver : public QObject {
    Q_OBJECT

public:
    explicit StreamReceiver(QObject *parent = nullptr);
    ~StreamReceiver();

    void connectToHost(const QString &host, int port);
    void disconnect();
    bool isConnected() const;

signals:
    void frameReceived(const FrameData &frame);
    void connected();
    void disconnected();
    void errorOccurred(const QString &error);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onError(QAbstractSocket::SocketError error);

private:
    bool parseFrame();

    QTcpSocket *m_socket;
    QByteArray m_buffer;

    static constexpr int HEADER_SIZE = 24;
    static constexpr const char MAGIC[] = "VCAM";
};
