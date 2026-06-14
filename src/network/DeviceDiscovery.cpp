#include "network/DeviceDiscovery.h"
#include "core/Logger.h"
#include <QNetworkDatagram>

DeviceDiscovery::DeviceDiscovery(QObject *parent)
    : QObject(parent)
    , m_socket(new QUdpSocket(this))
{
    connect(m_socket, &QUdpSocket::readyRead, this, &DeviceDiscovery::onReadyRead);
}

DeviceDiscovery::~DeviceDiscovery() {
    stop();
}

void DeviceDiscovery::start() {
    if (m_socket->state() == QAbstractSocket::BoundState) return;

    if (m_socket->bind(QHostAddress::AnyIPv4, BEACON_PORT, QUdpSocket::ShareAddress)) {
        VC_INFO("Discovery listening on UDP port {}", BEACON_PORT);
    } else {
        VC_ERROR("Failed to bind discovery socket on port {}: {}",
                 BEACON_PORT, m_socket->errorString().toStdString());
    }
}

void DeviceDiscovery::stop() {
    m_socket->close();
    VC_DEBUG("Discovery stopped");
}

void DeviceDiscovery::onReadyRead() {
    while (m_socket->hasPendingDatagrams()) {
        auto datagram = m_socket->receiveDatagram();
        auto payload = QString::fromUtf8(datagram.data());

        // Expected format: VIEWCAM|<name>|<tcpPort>|<deviceId>
        const auto parts = payload.split(QLatin1Char(vc::kBeaconSep));
        if (parts.size() < 4 || parts[0] != QLatin1String(vc::kBeaconTag)) {
            VC_TRACE("Ignoring non-VIEWCAM beacon from {}",
                     datagram.senderAddress().toString().toStdString());
            continue;
        }

        const QString name = parts[1];
        const int port = parts[2].toInt();
        const QString deviceId = parts[3];
        QString host = datagram.senderAddress().toString();

        // Strip IPv6 prefix if present (e.g., "::ffff:192.168.1.5")
        if (host.startsWith(QLatin1String("::ffff:")))
            host = host.mid(7);

        if (deviceId.isEmpty() || port <= 0) {
            VC_TRACE("Malformed beacon from {}", host.toStdString());
            continue;
        }

        VC_TRACE("Beacon: id={} {} at {}:{}", deviceId.toStdString(),
                 name.toStdString(), host.toStdString(), port);
        emit deviceFound(deviceId, name, host, port);
    }
}
