#include "ui/SettingsTab.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

static QGraphicsDropShadowEffect *createCardShadow(QWidget *parent) {
    auto *shadow = new QGraphicsDropShadowEffect(parent);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 30));
    shadow->setOffset(0, 2);
    return shadow;
}

static QFrame *createSettingsCard(QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName("settingsCard");
    card->setStyleSheet(
        "#settingsCard {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E8EAF0;"
        "  border-radius: 12px;"
        "  color: #3D4055;"
        "}"
        "#settingsCard * {"
        "  background-color: transparent;"
        "}"
    );
    card->setGraphicsEffect(createCardShadow(card));
    return card;
}

static QLabel *createSectionHeader(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setStyleSheet(
        "font-size: 11px; font-weight: 700; color: #8B90A0;"
        "letter-spacing: 1px; text-transform: uppercase;"
    );
    return label;
}

static QWidget *createSettingsRow(const QString &label, const QString &value, QWidget *parent) {
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 10, 0, 10);

    auto *labelWidget = new QLabel(label, row);
    labelWidget->setStyleSheet("font-size: 13px; color: #1A1A2E; font-weight: 500; border: none;");

    auto *valueWidget = new QLabel(value, row);
    valueWidget->setStyleSheet("font-size: 13px; color: #8B90A0; border: none;");

    layout->addWidget(labelWidget);
    layout->addStretch();
    layout->addWidget(valueWidget);

    return row;
}

static QFrame *createDivider(QWidget *parent) {
    auto *line = new QFrame(parent);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("border: none; border-top: 1px solid #F0F1F4;");
    line->setFixedHeight(1);
    return line;
}

SettingsTab::SettingsTab(Settings *settings, QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #ECEEF1;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(14);

    auto *header = new QLabel(tr("Settings"), this);
    header->setStyleSheet("font-size: 20px; font-weight: 700; color: #1A1A2E;");
    layout->addWidget(header);

    // Virtual Camera section
    layout->addWidget(createSectionHeader(tr("VIRTUAL CAMERA"), this));
    {
        auto *card = createSettingsCard(this);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 6, 20, 6);
        cardLayout->setSpacing(0);

#ifdef __linux__
        cardLayout->addWidget(createSettingsRow(tr("Backend"), "v4l2loopback", card));
        cardLayout->addWidget(createDivider(card));
        cardLayout->addWidget(createSettingsRow(tr("Device"), "/dev/video10", card));
#else
        cardLayout->addWidget(createSettingsRow(tr("Backend"), "DirectShow", card));
#endif
        layout->addWidget(card);
    }

    // Connection section
    layout->addWidget(createSectionHeader(tr("CONNECTION"), this));
    {
        auto *card = createSettingsCard(this);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 6, 20, 6);
        cardLayout->setSpacing(0);

        cardLayout->addWidget(createSettingsRow(tr("Default Port"),
            QString::number(settings->port()), card));
        cardLayout->addWidget(createDivider(card));
        cardLayout->addWidget(createSettingsRow(tr("Discovery"), "UDP Broadcast (:8081)", card));

        layout->addWidget(card);
    }

    // About section
    layout->addWidget(createSectionHeader(tr("ABOUT"), this));
    {
        auto *card = createSettingsCard(this);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 6, 20, 6);
        cardLayout->setSpacing(0);

        cardLayout->addWidget(createSettingsRow(tr("Version"), "0.1.0", card));
        cardLayout->addWidget(createDivider(card));
        cardLayout->addWidget(createSettingsRow(tr("Protocol"), "VCAM v1 (MJPEG)", card));
        cardLayout->addWidget(createDivider(card));
        cardLayout->addWidget(createSettingsRow("Qt", qVersion(), card));

        layout->addWidget(card);
    }

    layout->addStretch();
}
