#include "ui/AudioTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QGraphicsDropShadowEffect>

static QGraphicsDropShadowEffect *createCardShadow(QWidget *parent) {
    auto *shadow = new QGraphicsDropShadowEffect(parent);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 30));
    shadow->setOffset(0, 2);
    return shadow;
}

static QFrame *createAudioCard(const QString &icon, const QString &title,
                                const QString &description, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName("audioCard");
    card->setStyleSheet(
        "#audioCard {"
        "  background-color: #FFFFFF;"
        "  border: 1px solid #E8EAF0;"
        "  border-radius: 12px;"
        "  color: #3D4055;"
        "}"
        "#audioCard * {"
        "  background-color: transparent;"
        "}"
    );
    card->setGraphicsEffect(createCardShadow(card));

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(16);

    // Icon circle
    auto *iconContainer = new QFrame(card);
    iconContainer->setFixedSize(48, 48);
    iconContainer->setStyleSheet(
        "background-color: #F0F1F4;"
        "border-radius: 24px;"
        "border: none;"
    );
    auto *iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    auto *iconLabel = new QLabel(icon, iconContainer);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 22px; border: none; background: transparent;");
    iconLayout->addWidget(iconLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "font-size: 15px; font-weight: 700; color: #1A1A2E; border: none;"
    );

    auto *descLabel = new QLabel(description, card);
    descLabel->setStyleSheet(
        "font-size: 12px; color: #8B90A0; border: none;"
    );

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);

    auto *badge = new QLabel(QObject::tr("Coming Soon"), card);
    badge->setStyleSheet(
        "background-color: #FF8F00;"
        "color: white;"
        "border-radius: 10px;"
        "padding: 4px 12px;"
        "font-size: 11px;"
        "font-weight: 700;"
    );
    badge->setFixedHeight(24);

    layout->addWidget(iconContainer);
    layout->addLayout(textLayout, 1);
    layout->addWidget(badge);

    return card;
}

AudioTab::AudioTab(QWidget *parent)
    : QWidget(parent)
{
    setStyleSheet("background-color: #ECEEF1;");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(16);

    auto *header = new QLabel(tr("Audio Sources"), this);
    header->setStyleSheet("font-size: 20px; font-weight: 700; color: #1A1A2E;");

    auto *subtitle = new QLabel(tr("Audio streaming features are coming in a future release."), this);
    subtitle->setStyleSheet("font-size: 13px; color: #8B90A0;");

    layout->addWidget(header);
    layout->addWidget(subtitle);
    layout->addSpacing(8);
    layout->addWidget(createAudioCard(
        "\xF0\x9F\x8E\x99", tr("Microphone"),
        tr("Receive microphone audio from your phone"), this));
    layout->addWidget(createAudioCard(
        "\xF0\x9F\x94\x8A", tr("Speaker"),
        tr("Send desktop audio to your phone"), this));
    layout->addStretch();
}
