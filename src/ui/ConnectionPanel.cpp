/*
 * ConnectionPanel.cpp
 *
 * The "Devices Screen" content area:
 *   - Scrollable card list of discovered devices
 *   - ScanRippleWidget empty state (sonar animation)
 *   - Manual IP entry at the bottom
 *
 * Local Q_OBJECT classes (ScanRippleWidget, DeviceCard) are all defined before
 * the single #include "ConnectionPanel.moc" at the bottom of this file.
 * AUTOMOC processes all Q_OBJECT classes found in the translation unit.
 */

#include "ui/ConnectionPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QPainter>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QMouseEvent>
#include <QSizePolicy>
#include <QtMath>

// ─────────────────────────────────────────────────────────────────────────────
// ScanRippleWidget — sonar rings + pulsing center dot
// ─────────────────────────────────────────────────────────────────────────────
class ScanRippleWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScanRippleWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setFixedSize(180, 180);
        setAttribute(Qt::WA_TranslucentBackground);

        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, [this]() {
            m_elapsed += 30;
            update();
        });
        m_timer->start(30);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QPointF center(width() / 2.0, height() / 2.0);
        const double maxRadius = width() / 2.0 - 4.0;
        const int cycleDuration = 2400; // ms
        const int offsets[3] = { 0, 800, 1600 };
        const QColor accent(59, 130, 246); // #3B82F6

        // Three expanding rings with decaying opacity
        for (int i = 0; i < 3; i++) {
            int elapsed_i = m_elapsed - offsets[i];
            // Keep elapsed_i positive
            if (elapsed_i < 0) {
                elapsed_i = cycleDuration + (elapsed_i % cycleDuration);
            }
            double phase = static_cast<double>(elapsed_i % cycleDuration) / cycleDuration;

            double radius  = phase * maxRadius;
            double opacity = (1.0 - phase) * (1.0 - phase) * 0.55;

            QColor ringColor = accent;
            ringColor.setAlphaF(opacity);

            p.setPen(QPen(ringColor, 1.5));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(center, radius, radius);
        }

        // Center solid dot — pulsing alpha
        double pulse = 0.7 + 0.3 * qSin(m_elapsed * 0.004);
        QColor dotColor = accent;
        dotColor.setAlphaF(pulse);
        p.setPen(Qt::NoPen);
        p.setBrush(dotColor);
        p.drawEllipse(center, 6.0, 6.0);
    }

private:
    QTimer *m_timer   = nullptr;
    int     m_elapsed = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// DeviceCard — a single discovered-device row
// ─────────────────────────────────────────────────────────────────────────────
class DeviceCard : public QFrame {
    Q_OBJECT
public:
    explicit DeviceCard(const QString &name, const QString &host, int port,
                        QWidget *parent = nullptr)
        : QFrame(parent), m_name(name), m_host(host), m_port(port)
    {
        setObjectName("deviceCard");
        setFixedHeight(72);
        setCursor(Qt::PointingHandCursor);

        setStyleSheet(
            "QFrame#deviceCard {"
            "  background: #1C1C21;"
            "  border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 10px;"
            "}"
            "QFrame#deviceCard:hover {"
            "  background: #232329;"
            "  border: 1px solid rgba(59,130,246,0.35);"
            "}"
        );

        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);
        layout->setSpacing(12);

        // Phone icon
        m_iconLabel = new QLabel(QString::fromUtf8("\xF0\x9F\x93\xB1"), this);
        m_iconLabel->setFixedSize(32, 32);
        m_iconLabel->setAlignment(Qt::AlignCenter);
        m_iconLabel->setStyleSheet(
            "font-size: 18px;"
            "background: rgba(59,130,246,0.12);"
            "border-radius: 8px;"
            "border: none;"
        );
        layout->addWidget(m_iconLabel);

        // Name + IP column
        auto *textCol = new QVBoxLayout();
        textCol->setSpacing(2);
        textCol->setContentsMargins(0, 0, 0, 0);

        m_nameLabel = new QLabel(name, this);
        m_nameLabel->setStyleSheet(
            "font-size: 14px;"
            "font-weight: 600;"
            "color: rgba(255,255,255,0.92);"
            "background: transparent;"
            "border: none;"
        );

        m_addrLabel = new QLabel(QString("%1:%2").arg(host).arg(port), this);
        m_addrLabel->setStyleSheet(
            "font-family: 'JetBrains Mono', 'Fira Code', monospace;"
            "font-size: 11px;"
            "color: rgba(255,255,255,0.34);"
            "background: transparent;"
            "border: none;"
        );

        textCol->addStretch();
        textCol->addWidget(m_nameLabel);
        textCol->addWidget(m_addrLabel);
        textCol->addStretch();
        layout->addLayout(textCol, 1);

        // Chevron
        m_chevron = new QLabel(">", this);
        m_chevron->setStyleSheet(
            "font-size: 16px;"
            "color: rgba(255,255,255,0.34);"
            "background: transparent;"
            "border: none;"
        );
        layout->addWidget(m_chevron);
    }

    QString name() const { return m_name; }
    QString host() const { return m_host; }
    int     port() const { return m_port; }

    void setConnectingState() {
        m_nameLabel->setText(m_name + "  \xe2\x80\x94  Connecting...");
        m_nameLabel->setStyleSheet(
            "font-size: 14px;"
            "font-weight: 600;"
            "color: rgba(59,130,246,0.90);"
            "background: transparent;"
            "border: none;"
        );
        m_chevron->setText("...");
    }

    void setConnectedState() {
        setStyleSheet(
            "QFrame#deviceCard {"
            "  background: rgba(34,197,94,0.08);"
            "  border: 1px solid rgba(34,197,94,0.45);"
            "  border-radius: 10px;"
            "}"
        );
        // Unicode checkmark
        m_chevron->setText(QString::fromUtf8("\xe2\x9c\x93"));
        m_chevron->setStyleSheet(
            "font-size: 16px;"
            "color: #22C55E;"
            "background: transparent;"
            "border: none;"
        );
    }

signals:
    void clicked(const QString &name, const QString &host, int port);

protected:
    void mousePressEvent(QMouseEvent *) override {
        emit clicked(m_name, m_host, m_port);
    }

private:
    QString  m_name;
    QString  m_host;
    int      m_port;
    QLabel  *m_iconLabel  = nullptr;
    QLabel  *m_nameLabel  = nullptr;
    QLabel  *m_addrLabel  = nullptr;
    QLabel  *m_chevron    = nullptr;
};

// AUTOMOC requires this include to be after ALL Q_OBJECT classes in this file.
#include "ConnectionPanel.moc"

// ─────────────────────────────────────────────────────────────────────────────
// ConnectionPanel
// ─────────────────────────────────────────────────────────────────────────────

ConnectionPanel::ConnectionPanel(QWidget *parent)
    : QWidget(parent)
{
    // No scroll area — plain vertical layout:
    //   [section header]
    //   [device cards]       ← grows as devices are found
    //   [empty state]        ← visible when no cards; hidden otherwise
    //   [stretch]            ← pushes manual section to bottom
    //   [manual entry]       ← always at the bottom

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Section header row ───────────────────────────────────────────────────
    auto *sectionRow = new QHBoxLayout();
    sectionRow->setContentsMargins(0, 0, 0, 12);
    sectionRow->setSpacing(8);

    auto *sectionLabel = new QLabel("AVAILABLE DEVICES", this);
    sectionLabel->setStyleSheet(
        "font-size: 10px; font-weight: 700; letter-spacing: 1.2px;"
        "color: rgba(255,255,255,0.34); background: transparent; border: none;"
    );

    // Pulsing dot + "Scanning…"
    auto *scanIndicator = new QWidget(this);
    scanIndicator->setStyleSheet("background: transparent;");
    auto *scanRow = new QHBoxLayout(scanIndicator);
    scanRow->setContentsMargins(0, 0, 0, 0);
    scanRow->setSpacing(5);

    auto *scanDot = new QLabel(scanIndicator);
    scanDot->setFixedSize(7, 7);
    scanDot->setStyleSheet("background: #3B82F6; border-radius: 3px; border: none;");
    auto *dotEffect = new QGraphicsOpacityEffect(scanDot);
    scanDot->setGraphicsEffect(dotEffect);
    auto *dotAnim = new QPropertyAnimation(dotEffect, "opacity", scanDot);
    dotAnim->setDuration(1000);
    dotAnim->setStartValue(1.0);
    dotAnim->setEndValue(0.2);
    dotAnim->setLoopCount(-1);
    dotAnim->start();

    auto *scanText = new QLabel("Scanning...", scanIndicator);
    scanText->setStyleSheet(
        "font-size: 11px; color: rgba(255,255,255,0.34);"
        "background: transparent; border: none;"
    );
    scanRow->addWidget(scanDot);
    scanRow->addWidget(scanText);

    sectionRow->addWidget(sectionLabel);
    sectionRow->addStretch();
    sectionRow->addWidget(scanIndicator);
    root->addLayout(sectionRow);

    // ── Device cards ─────────────────────────────────────────────────────────
    m_cardsContainer = new QWidget(this);
    m_cardsContainer->setStyleSheet("background: transparent;");
    m_cardsLayout = new QVBoxLayout(m_cardsContainer);
    m_cardsLayout->setContentsMargins(0, 0, 0, 0);
    m_cardsLayout->setSpacing(8);
    m_cardsLayout->setAlignment(Qt::AlignTop);
    root->addWidget(m_cardsContainer);

    // ── Empty state ───────────────────────────────────────────────────────────
    m_emptyState = new QWidget(this);
    m_emptyState->setStyleSheet("background: transparent;");
    auto *emptyLayout = new QVBoxLayout(m_emptyState);
    emptyLayout->setContentsMargins(0, 40, 0, 40);
    emptyLayout->setSpacing(14);
    emptyLayout->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    auto *ripple = new ScanRippleWidget(m_emptyState);
    emptyLayout->addWidget(ripple, 0, Qt::AlignHCenter);

    auto *emptyTitle = new QLabel("Looking for devices on your network", m_emptyState);
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setWordWrap(true);
    emptyTitle->setStyleSheet(
        "font-size: 14px; font-weight: 500; color: rgba(255,255,255,0.56);"
        "background: transparent; border: none;"
    );

    auto *emptySubtitle = new QLabel("Make sure ViewCam is open on your phone", m_emptyState);
    emptySubtitle->setAlignment(Qt::AlignCenter);
    emptySubtitle->setWordWrap(true);
    emptySubtitle->setStyleSheet(
        "font-size: 12px; color: rgba(255,255,255,0.34);"
        "background: transparent; border: none;"
    );

    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptySubtitle);
    root->addWidget(m_emptyState, 1); // stretch=1 → fills available space

    // ── Stretch (pushes manual entry to bottom when cards exist) ─────────────
    root->addStretch(1);

    // ── Manual connect ────────────────────────────────────────────────────────
    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    separator->setStyleSheet(
        "border: none; border-top: 1px solid rgba(255,255,255,0.06); background: transparent;"
    );
    root->addWidget(separator);

    auto *manualLabel = new QLabel("Or connect manually", this);
    manualLabel->setStyleSheet(
        "font-size: 12px; color: rgba(255,255,255,0.34);"
        "background: transparent; border: none;"
    );
    root->addSpacing(12);
    root->addWidget(manualLabel);
    root->addSpacing(8);

    auto *inputRow = new QHBoxLayout();
    inputRow->setSpacing(8);

    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setObjectName("manualHostEdit");
    m_hostEdit->setPlaceholderText("IP address (e.g. 192.168.1.10)");
    m_hostEdit->setFixedHeight(38);
    m_hostEdit->setStyleSheet(
        "QLineEdit#manualHostEdit {"
        "  background: #1C1C21; color: rgba(255,255,255,0.92);"
        "  border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
        "  padding: 0 12px; font-size: 13px;"
        "}"
        "QLineEdit#manualHostEdit:focus { border: 1px solid rgba(59,130,246,0.55); }"
    );
    connect(m_hostEdit, &QLineEdit::returnPressed,
            this, &ConnectionPanel::onManualConnectClicked);

    m_connectBtn = new QPushButton("Connect", this);
    m_connectBtn->setObjectName("manualConnectBtn");
    m_connectBtn->setFixedSize(88, 38);
    m_connectBtn->setCursor(Qt::PointingHandCursor);
    m_connectBtn->setStyleSheet(
        "QPushButton#manualConnectBtn {"
        "  background: #3B82F6; color: white; border: none;"
        "  border-radius: 8px; font-size: 13px; font-weight: 600;"
        "}"
        "QPushButton#manualConnectBtn:hover { background: #60A5FA; }"
        "QPushButton#manualConnectBtn:pressed { background: #2563EB; }"
    );
    connect(m_connectBtn, &QPushButton::clicked,
            this, &ConnectionPanel::onManualConnectClicked);

    inputRow->addWidget(m_hostEdit, 1);
    inputRow->addWidget(m_connectBtn);
    root->addLayout(inputRow);
    root->addSpacing(4);

    updateEmptyState();
}

// ─────────────────────────────────────────────────────────────────────────────

void ConnectionPanel::addDevice(const QString &name, const QString &host, int port) {
    // Deduplicate
    for (const auto &e : m_entries) {
        if (e.host == host && e.port == port)
            return;
    }
    m_entries.append({ name, host, port });

    auto *card = static_cast<DeviceCard *>(createDeviceCard(name, host, port));
    m_cardsLayout->addWidget(card);

    // Entrance fade animation
    auto *effect = new QGraphicsOpacityEffect(card);
    effect->setOpacity(0.0);
    card->setGraphicsEffect(effect);

    auto *anim = new QPropertyAnimation(effect, "opacity", card);
    anim->setDuration(280);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);

    updateEmptyState();
}

void ConnectionPanel::setConnected(bool connected) {
    m_connected = connected;
    if (!connected) {
        m_connectedHost.clear();
        m_connectedPort = 0;
    }
}

void ConnectionPanel::setFps(int fps) {
    Q_UNUSED(fps)
    // FPS is displayed in MainWindow's camera screen chips.
}

void ConnectionPanel::showSessionLimitMessage() {
    // Surfaced via MainWindow::setStatusText in Application.cpp.
}

void ConnectionPanel::triggerDisconnect() {
    emit disconnectRequested();
}

void ConnectionPanel::onManualConnectClicked() {
    const QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty())
        return;
    // Device name defaults to host when manually entered
    m_connectedHost = host;
    m_connectedPort = 8080;
    emit connectRequested(host, 8080);
}

void ConnectionPanel::updateEmptyState() {
    m_emptyState->setVisible(m_entries.isEmpty());
}

QWidget *ConnectionPanel::createDeviceCard(const QString &name,
                                            const QString &host,
                                            int port)
{
    auto *card = new DeviceCard(name, host, port, m_cardsContainer);

    connect(card, &DeviceCard::clicked, this,
            [this, card](const QString &n, const QString &h, int p) {
        m_connectedHost = h;
        m_connectedPort = p;
        card->setConnectingState();
        emit deviceCardClicked(n, h, p);
        emit connectRequested(h, p);
    });

    return card;
}
