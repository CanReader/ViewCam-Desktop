#include "ui/AudioTab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QFont>
#include <QScrollArea>

static QFrame *createAudioCard(const QString &icon, const QString &title,
                                const QString &description, QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName("audioCard");

    auto *layout = new QHBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(16);

    // Icon circle
    auto *iconContainer = new QFrame(card);
    iconContainer->setObjectName("audioIcon");
    iconContainer->setFixedSize(48, 48);
    auto *iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    auto *iconLabel = new QLabel(icon, iconContainer);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 22px; border: none; background: transparent;");
    iconLayout->addWidget(iconLabel);

    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(4);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("audioTitle");

    auto *descLabel = new QLabel(description, card);
    descLabel->setObjectName("audioDesc");

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);

    auto *badge = new QLabel(QObject::tr("Coming Soon"), card);
    badge->setObjectName("comingSoonBadge");
    badge->setFixedHeight(24);

    layout->addWidget(iconContainer);
    layout->addLayout(textLayout, 1);
    layout->addWidget(badge);

    return card;
}

AudioTab::AudioTab(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("audioPage");

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setObjectName("audioPage");

    auto *scrollContent = new QWidget();
    scrollContent->setObjectName("audioPage");
    auto *layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(16);

    auto *header = new QLabel(tr("Audio Sources"), scrollContent);
    header->setObjectName("pageHeader");

    auto *subtitle = new QLabel(tr("Audio streaming features are coming in a future release."), scrollContent);
    subtitle->setObjectName("pageSubtitle");

    layout->addWidget(header);
    layout->addWidget(subtitle);
    layout->addSpacing(8);
    layout->addWidget(createAudioCard(
        "\xF0\x9F\x8E\x99", tr("Microphone"),
        tr("Receive microphone audio from your phone"), scrollContent));
    layout->addWidget(createAudioCard(
        "\xF0\x9F\x94\x8A", tr("Speaker"),
        tr("Send desktop audio to your phone"), scrollContent));
    layout->addStretch();

    scrollArea->setWidget(scrollContent);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
}
