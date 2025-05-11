#include "ui/ConnectionPanel.h"
#include <QHBoxLayout>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);

    m_deviceCombo = new QComboBox(this);
    m_deviceCombo->setMinimumWidth(150);
    m_deviceCombo->addItem("Manual...", QVariant());
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionPanel::onDeviceSelected);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText("IP address");
    m_hostEdit->setMinimumWidth(120);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);

    m_connectBtn = new QPushButton("Connect", this);
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);

    m_statusLabel = new QLabel("Disconnected", this);
    m_fpsLabel = new QLabel("", this);

    layout->addWidget(m_deviceCombo);
    layout->addWidget(m_hostEdit);
    layout->addWidget(m_portSpin);
    layout->addWidget(m_connectBtn);
    layout->addStretch();
    layout->addWidget(m_fpsLabel);
    layout->addWidget(m_statusLabel);
}

void ConnectionPanel::addDevice(const QString &name, const QString &host, int port) {
    QString key = host + ":" + QString::number(port);
    // Avoid duplicates
    for (int i = 0; i < m_deviceCombo->count(); ++i) {
        if (m_deviceCombo->itemData(i).toString() == key) {
            return;
        }
    }
    QString label = QString("%1 (%2:%3)").arg(name, host).arg(port);
    m_deviceCombo->addItem(label, key);
    // auto-select first one
    if (m_deviceCombo->count() == 2) {
        m_deviceCombo->setCurrentIndex(1);
    }
}

void ConnectionPanel::setConnected(bool connected) {
    m_connected = connected;
    m_connectBtn->setText(connected ? "Disconnect" : "Connect");
    m_statusLabel->setText(connected ? "Connected" : "Disconnected");
    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_deviceCombo->setEnabled(!connected);
}

void ConnectionPanel::setFps(int fps) {
    m_fpsLabel->setText(fps > 0 ? QString("%1 FPS").arg(fps) : "");
}

void ConnectionPanel::onConnectClicked() {
    if (m_connected) {
        emit disconnectRequested();
    } else {
        emit connectRequested(m_hostEdit->text(), m_portSpin->value());
    }
}

void ConnectionPanel::onDeviceSelected(int index) {
    auto data = m_deviceCombo->itemData(index).toString();
    if (data.isEmpty()) return; // "Manual..." selected

    auto parts = data.split(':');
    if (parts.size() == 2) {
        m_hostEdit->setText(parts[0]);
        m_portSpin->setValue(parts[1].toInt());
    }
}
