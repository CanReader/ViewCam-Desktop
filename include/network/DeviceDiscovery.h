#pragma once

#include <QObject>
#include <QUdpSocket>
#include "core/Constants.h"

struct DiscoveredDevice {
    QString deviceId;
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
    // host is taken from the UDP packet source address, not the beacon body.
    void deviceFound(const QString &deviceId, const QString &name,
                     const QString &host, int port);

private slots:
    void onReadyRead();

private:
    QUdpSocket *m_socket;
    static constexpr int BEACON_PORT = vc::kBeaconPort;
};
