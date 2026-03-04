#include "ui/SettingsTab.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QPushButton>
#ifdef _WIN32
#include "virtualcam/FilterRegistrar.h"
#endif

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
#elif defined(_WIN32)
        cardLayout->addWidget(createSettingsRow(tr("Backend"), "DirectShow", card));
        cardLayout->addWidget(createDivider(card));

        // Filter Status row
        {
            auto *row = new QWidget(card);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 10, 0, 10);

            auto *label = new QLabel(tr("Filter Status"), row);
            label->setObjectName("settingsLabel");

            m_filterStatusLabel = new QLabel(tr("Checking..."), row);
            m_filterStatusLabel->setObjectName("settingsValue");

            rowLayout->addWidget(label);
            rowLayout->addStretch();
            rowLayout->addWidget(m_filterStatusLabel);
            cardLayout->addWidget(row);
        }
        cardLayout->addWidget(createDivider(card));

        // Filter DLL path row
        {
            auto *row = new QWidget(card);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 10, 0, 10);

            auto *label = new QLabel(tr("Filter DLL"), row);
            label->setObjectName("settingsLabel");

            m_filterPathLabel = new QLabel("", row);
            m_filterPathLabel->setObjectName("settingsValue");
            m_filterPathLabel->setWordWrap(true);

            rowLayout->addWidget(label);
            rowLayout->addStretch();
            rowLayout->addWidget(m_filterPathLabel);
            cardLayout->addWidget(row);
        }
        cardLayout->addWidget(createDivider(card));

        // Install / Uninstall buttons row
        {
            auto *row = new QWidget(card);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 10, 0, 10);

            m_installBtn = new QPushButton(tr("Install Filter"), row);
            m_installBtn->setObjectName("connectBtn");
            m_installBtn->setCursor(Qt::PointingHandCursor);
            m_installBtn->setFixedHeight(34);
            m_installBtn->setFixedWidth(130);

            m_uninstallBtn = new QPushButton(tr("Uninstall Filter"), row);
            m_uninstallBtn->setObjectName("disconnectBtn");
            m_uninstallBtn->setCursor(Qt::PointingHandCursor);
            m_uninstallBtn->setFixedHeight(34);
            m_uninstallBtn->setFixedWidth(130);

            connect(m_installBtn, &QPushButton::clicked, this, [this]() {
                m_installBtn->setEnabled(false);
                m_filterStatusLabel->setText(tr("Registering..."));
                FilterRegistrar::registerFilter();
                updateFilterStatus();
                m_installBtn->setEnabled(true);
            });

            connect(m_uninstallBtn, &QPushButton::clicked, this, [this]() {
                m_uninstallBtn->setEnabled(false);
                m_filterStatusLabel->setText(tr("Unregistering..."));
                FilterRegistrar::unregisterFilter();
                updateFilterStatus();
                m_uninstallBtn->setEnabled(true);
            });

            rowLayout->addStretch();
            rowLayout->addWidget(m_installBtn);
            rowLayout->addSpacing(8);
            rowLayout->addWidget(m_uninstallBtn);
            cardLayout->addWidget(row);
        }
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

#ifdef _WIN32
void SettingsTab::updateFilterStatus() {
    auto status = FilterRegistrar::checkStatus();
    QString text = FilterRegistrar::statusText(status);

    // Color-code the status text
    switch (status) {
    case FilterRegistrar::Status::Registered:
        m_filterStatusLabel->setStyleSheet("color: #22c55e; font-weight: 600;");
        break;
    case FilterRegistrar::Status::RegisteredStale:
        m_filterStatusLabel->setStyleSheet("color: #fbbf24; font-weight: 600;");
        break;
    case FilterRegistrar::Status::NotRegistered:
        m_filterStatusLabel->setStyleSheet("color: #ef4444; font-weight: 600;");
        break;
    }
    m_filterStatusLabel->setText(text);

    // Show registered path or expected path
    QString regPath = FilterRegistrar::registeredDllPath();
    if (regPath.isEmpty()) {
        m_filterPathLabel->setText(FilterRegistrar::expectedDllPath());
    } else {
        m_filterPathLabel->setText(regPath);
    }

    // Enable/disable buttons based on status
    bool installed = (status == FilterRegistrar::Status::Registered);
    m_installBtn->setEnabled(!installed);
    m_uninstallBtn->setEnabled(status != FilterRegistrar::Status::NotRegistered);
}
#endif
