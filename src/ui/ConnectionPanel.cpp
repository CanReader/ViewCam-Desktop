#include "ui/ConnectionPanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFrame>
#include <QFont>
#include <QStyle>
#include <QGraphicsDropShadowEffect>

static const char *CARD_STYLE =
    "QFrame#connectionCard {"
    "  background-color: #FFFFFF;"
    "  border: 1px solid #E8EAF0;"
    "  border-radius: 12px;"
    "  color: #1A1A2E;"
    "}"
    "QFrame#connectionCard * {"
    "  background-color: transparent;"
    "}"
    "QFrame#connectionCard QLabel {"
    "  border: none;"
    "  color: #3D4055;"
    "}"
    "QFrame#connectionCard QComboBox,"
    "QFrame#connectionCard QLineEdit,"
    "QFrame#connectionCard QSpinBox {"
    "  border: 1px solid #DDE0E6;"
    "  border-radius: 6px;"
    "  padding: 8px 10px;"
    "  background: #F5F6F8;"
    "  color: #1A1A2E;"
    "  font-size: 13px;"
    "}"
    "QFrame#connectionCard QComboBox:hover,"
    "QFrame#connectionCard QLineEdit:hover,"
    "QFrame#connectionCard QSpinBox:hover {"
    "  border: 1px solid #B8BCC8;"
    "}"
    "QFrame#connectionCard QComboBox:focus,"
    "QFrame#connectionCard QLineEdit:focus,"
    "QFrame#connectionCard QSpinBox:focus {"
    "  border: 2px solid #00897B;"
    "  padding: 7px 9px;"
    "}"
    "QFrame#connectionCard QComboBox QAbstractItemView {"
    "  background: #FFFFFF;"
    "  color: #1A1A2E;"
    "  border: 1px solid #DDE0E6;"
    "  selection-background-color: #E0F2F1;"
    "  selection-color: #00695C;"
    "}"
    "QFrame#connectionCard QPushButton#connectBtn {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "    stop:0 #00897B, stop:1 #00796B);"
    "  color: white;"
    "  border: none;"
    "  border-radius: 8px;"
    "  padding: 10px 32px;"
    "  font-size: 13px;"
    "  font-weight: 700;"
    "}"
    "QFrame#connectionCard QPushButton#connectBtn:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "    stop:0 #00796B, stop:1 #00695C);"
    "}"
    "QFrame#connectionCard QPushButton#connectBtn:pressed {"
    "  background: #00695C;"
    "}"
    "QFrame#connectionCard QPushButton#disconnectBtn {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "    stop:0 #E53935, stop:1 #D32F2F);"
    "  color: white;"
    "  border: none;"
    "  border-radius: 8px;"
    "  padding: 10px 32px;"
    "  font-size: 13px;"
    "  font-weight: 700;"
    "}"
    "QFrame#connectionCard QPushButton#disconnectBtn:hover {"
    "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
    "    stop:0 #D32F2F, stop:1 #C62828);"
    "}";

static QGraphicsDropShadowEffect *createCardShadow(QWidget *parent) {
    auto *shadow = new QGraphicsDropShadowEffect(parent);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 30));
    shadow->setOffset(0, 2);
    return shadow;
}

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(16);

    // ── Discovered Devices card ──
    auto *devicesCard = new QFrame(this);
    devicesCard->setObjectName("connectionCard");
    devicesCard->setStyleSheet(CARD_STYLE);
    devicesCard->setGraphicsEffect(createCardShadow(devicesCard));

    auto *devicesLayout = new QVBoxLayout(devicesCard);
    devicesLayout->setContentsMargins(20, 16, 20, 16);
    devicesLayout->setSpacing(10);

    auto *devicesHeader = new QLabel(tr("Discovered Devices"), devicesCard);
    devicesHeader->setStyleSheet(
        "font-size: 14px; font-weight: 700; color: #1A1A2E; border: none;"
    );

    m_deviceCombo = new QComboBox(devicesCard);
    m_deviceCombo->setMinimumWidth(200);
    m_deviceCombo->setMinimumHeight(36);
    m_deviceCombo->addItem(tr("Manual..."), QVariant());
    connect(m_deviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConnectionPanel::onDeviceSelected);

    m_fpsLabel = new QLabel("", devicesCard);
    m_fpsLabel->setStyleSheet(
        "font-size: 13px; color: #00897B; font-weight: 700; border: none;"
    );

    m_statusLabel = new QLabel(tr("Disconnected"), devicesCard);
    m_statusLabel->setStyleSheet(
        "font-size: 13px; color: #8B90A0; border: none;"
    );

    devicesLayout->addWidget(devicesHeader);
    devicesLayout->addWidget(m_deviceCombo);
    devicesLayout->addStretch();
    auto *statusRow = new QHBoxLayout();
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();
    statusRow->addWidget(m_fpsLabel);
    devicesLayout->addLayout(statusRow);

    outerLayout->addWidget(devicesCard, 1);

    // ── Connect by Address card ──
    auto *addressCard = new QFrame(this);
    addressCard->setObjectName("connectionCard");
    addressCard->setStyleSheet(CARD_STYLE);
    addressCard->setGraphicsEffect(createCardShadow(addressCard));

    auto *addressLayout = new QVBoxLayout(addressCard);
    addressLayout->setContentsMargins(20, 16, 20, 16);
    addressLayout->setSpacing(10);

    auto *addressHeader = new QLabel(tr("Connect by Address"), addressCard);
    addressHeader->setStyleSheet(
        "font-size: 14px; font-weight: 700; color: #1A1A2E; border: none;"
    );

    m_hostEdit = new QLineEdit(addressCard);
    m_hostEdit->setPlaceholderText(tr("IP address (e.g. 192.168.1.100)"));
    m_hostEdit->setMinimumHeight(36);

    m_portSpin = new QSpinBox(addressCard);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(8080);
    m_portSpin->setPrefix(tr("Port: "));
    m_portSpin->setMinimumHeight(36);

    m_connectBtn = new QPushButton(tr("Connect"), addressCard);
    m_connectBtn->setObjectName("connectBtn");
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setMinimumHeight(40);
    connect(m_connectBtn, &QPushButton::clicked, this, &ConnectionPanel::onConnectClicked);

    addressLayout->addWidget(addressHeader);
    addressLayout->addWidget(m_hostEdit);
    addressLayout->addWidget(m_portSpin);
    addressLayout->addStretch();
    addressLayout->addWidget(m_connectBtn);

    outerLayout->addWidget(addressCard, 1);
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
    m_statusLabel->setStyleSheet(connected
        ? "font-size: 13px; color: #00897B; font-weight: 700; border: none;"
        : "font-size: 13px; color: #8B90A0; border: none;");
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

void ConnectionPanel::onDeviceSelected(int index) {
    auto data = m_deviceCombo->itemData(index).toString();
    if (data.isEmpty()) return;

    auto parts = data.split(':');
    if (parts.size() == 2) {
        m_hostEdit->setText(parts[0]);
        m_portSpin->setValue(parts[1].toInt());
    }
}
