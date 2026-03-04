#include "ui/ConnectionPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>
#include <QStyle>

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *card = new QFrame(this);
    card->setObjectName("connectionCard");

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(card);

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    // Device header row (dot + name)
    auto *headerRow = new QHBoxLayout();
    headerRow->setSpacing(8);

    m_deviceDot = new QLabel(card);
    m_deviceDot->setObjectName("deviceDot");
    m_deviceDot->setFixedSize(8, 8);

    m_deviceNameLabel = new QLabel(tr("No Device"), card);
    m_deviceNameLabel->setObjectName("deviceName");

    headerRow->addWidget(m_deviceDot);
    headerRow->addWidget(m_deviceNameLabel, 1);
    layout->addLayout(headerRow);

    // Device combo
    m_deviceCombo = new QComboBox(card);
    m_deviceCombo->setMinimumHeight(32);
    m_deviceCombo->addItem(tr("Manual..."), QVariant());
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionPanel::onDeviceSelected);
    layout->addWidget(m_deviceCombo);

    // IP + Port
    m_hostEdit = new QLineEdit(card);
    m_hostEdit->setPlaceholderText(tr("IP address"));
    m_hostEdit->setMinimumHeight(32);
    layout->addWidget(m_hostEdit);

    m_portSpin = new QSpinBox(card);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    m_portSpin->setPrefix(tr("Port: "));
    m_portSpin->setMinimumHeight(32);
    layout->addWidget(m_portSpin);

    // Status row
    auto *statusRow = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Disconnected"), card);
    m_statusLabel->setObjectName("statusLabel");
    m_fpsLabel = new QLabel("", card);
    m_fpsLabel->setObjectName("fpsLabel");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    statusRow->addWidget(m_fpsLabel);
    layout->addLayout(statusRow);

    // Connect button
    m_connectBtn = new QPushButton(tr("Connect"), card);
    m_connectBtn->setObjectName("connectBtn");
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setMinimumHeight(34);
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);
    layout->addWidget(m_connectBtn);
}

void ConnectionPanel::addDevice(const QString &name, const QString &host, int port) {
    QString key = host + ":" + QString::number(port);
    for (int i = 0; i < m_deviceCombo->count(); ++i) {
        if (m_deviceCombo->itemData(i).toString() == key) {
            return;
        }
    }
    QString label = QString("%1 (%2:%3)").arg(name, host).arg(port);
    m_deviceCombo->addItem(label, key);
    if (m_deviceCombo->count() == 2) {
        m_deviceCombo->setCurrentIndex(1);
    }
}

void ConnectionPanel::setConnected(bool connected) {
    m_connected = connected;
    m_connectBtn->setText(connected ? tr("Disconnect") : tr("Connect"));
    m_connectBtn->setObjectName(connected ? "disconnectBtn" : "connectBtn");
    m_connectBtn->style()->unpolish(m_connectBtn);
    m_connectBtn->style()->polish(m_connectBtn);

    m_statusLabel->setText(connected ? tr("Connected") : tr("Disconnected"));
    m_deviceNameLabel->setText(connected
        ? m_deviceCombo->currentText().section('(', 0, 0).trimmed()
        : tr("No Device"));

    m_hostEdit->setEnabled(!connected);
    m_portSpin->setEnabled(!connected);
    m_deviceCombo->setEnabled(!connected);
}

void ConnectionPanel::setFps(int fps) {
    m_fpsLabel->setText(fps > 0 ? tr("%1 FPS").arg(fps) : "");
}

void ConnectionPanel::onConnectClicked() {
    if (m_connected) {
        emit disconnectRequested();
    } else {
        emit connectRequested(m_hostEdit->text(), m_portSpin->value());
    }
}

void ConnectionPanel::showSessionLimitMessage() {
    m_statusLabel->setText(tr("Session ended (Free tier limit)"));
    m_statusLabel->setStyleSheet("color: #FFB800; font-weight: bold;");
}

void ConnectionPanel::onDeviceSelected(int index) {
    auto data = m_deviceCombo->itemData(index).toString();
    if (data.isEmpty()) return;

    auto parts = data.split(':');
    if (parts.size() == 2) {
        m_hostEdit->setText(parts[0]);
        m_portSpin->setValue(parts[1].toInt());
    }
}
