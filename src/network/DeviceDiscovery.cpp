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

        // Expected format: VIEWCAM|<name>|<port>
        auto parts = payload.split('|');
        if (parts.size() != 3 || parts[0] != "VIEWCAM") {
            VC_TRACE("Ignoring non-VIEWCAM beacon from {}",
                     datagram.senderAddress().toString().toStdString());
            continue;
        }

        QString name = parts[1];
        int port = parts[2].toInt();
        QString host = datagram.senderAddress().toString();

        // Strip IPv6 prefix if present (e.g., "::ffff:192.168.1.5")
        if (host.startsWith("::ffff:")) {
            host = host.mid(7);
        }

        VC_DEBUG("Beacon from {} at {}:{}", name.toStdString(), host.toStdString(), port);
        emit deviceFound(name, host, port);
    }
}
