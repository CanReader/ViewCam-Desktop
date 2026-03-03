#include "ui/SettingsTab.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>

static QFrame *createSettingsCard(QWidget *parent) {
    auto *card = new QFrame(parent);
    card->setObjectName("settingsCard");
    return card;
}

static QLabel *createSectionHeader(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("sectionHeader");
    return label;
}

static QWidget *createSettingsRow(const QString &label, const QString &value, QWidget *parent) {
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 10, 0, 10);

    auto *labelWidget = new QLabel(label, row);
    labelWidget->setObjectName("settingsLabel");

    auto *valueWidget = new QLabel(value, row);
    valueWidget->setObjectName("settingsValue");

    layout->addWidget(labelWidget);
    layout->addStretch();
    layout->addWidget(valueWidget);

    return row;
}

static QFrame *createDivider(QWidget *parent) {
    auto *line = new QFrame(parent);
    line->setObjectName("settingsDivider");
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    return line;
}

SettingsTab::SettingsTab(Settings *settings, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("settingsPage");

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setObjectName("settingsPage");

    auto *scrollContent = new QWidget();
    scrollContent->setObjectName("settingsPage");
    auto *layout = new QVBoxLayout(scrollContent);
    layout->setContentsMargins(24, 28, 24, 24);
    layout->setSpacing(14);

    auto *header = new QLabel(tr("Settings"), scrollContent);
    header->setObjectName("pageHeader");
    layout->addWidget(header);

    // Appearance section
    layout->addWidget(createSectionHeader(tr("APPEARANCE"), scrollContent));
    {
        auto *card = createSettingsCard(scrollContent);
        auto *cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(20, 6, 20, 6);
        cardLayout->setSpacing(0);

        auto *themeRow = new QWidget(card);
        auto *themeRowLayout = new QHBoxLayout(themeRow);
        themeRowLayout->setContentsMargins(0, 10, 0, 10);

        auto *themeLabel = new QLabel(tr("Theme"), themeRow);
        themeLabel->setObjectName("settingsLabel");

        m_themeCombo = new QComboBox(themeRow);
        m_themeCombo->addItem(tr("Auto"));
        m_themeCombo->addItem(tr("Dark"));
        m_themeCombo->addItem(tr("Light"));
        m_themeCombo->setMinimumHeight(32);
        m_themeCombo->setMinimumWidth(100);

        connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &SettingsTab::themeChanged);

        themeRowLayout->addWidget(themeLabel);
        themeRowLayout->addStretch();
        themeRowLayout->addWidget(m_themeCombo);

        cardLayout->addWidget(themeRow);
        layout->addWidget(card);
    }

    // Virtual Camera section
    layout->addWidget(createSectionHeader(tr("VIRTUAL CAMERA"), scrollContent));
    {
        auto *card = createSettingsCard(scrollContent);
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
    layout->addWidget(createSectionHeader(tr("CONNECTION"), scrollContent));
    {
        auto *card = createSettingsCard(scrollContent);
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
    layout->addWidget(createSectionHeader(tr("ABOUT"), scrollContent));
    {
        auto *card = createSettingsCard(scrollContent);
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

    scrollArea->setWidget(scrollContent);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
}

void SettingsTab::setThemeIndex(int index) {
    m_themeCombo->blockSignals(true);
    m_themeCombo->setCurrentIndex(index);
    m_themeCombo->blockSignals(false);
}
