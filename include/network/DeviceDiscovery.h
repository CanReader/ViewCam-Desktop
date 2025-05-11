#pragma once

#include <QObject>
#include <QUdpSocket>

struct DiscoveredDevice {
    QString name;
    QString host;
    int port;
};

class DeviceDiscovery : public QObject {
    Q_OBJECT

public:
    explicit DeviceDiscovery(QObject *parent = nullptr);
    ~DeviceDiscovery();

    void start();
    void stop();

signals:
    void deviceFound(const QString &name, const QString &host, int port);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket;
    static constexpr int BEACON_PORT = 8081;
};
