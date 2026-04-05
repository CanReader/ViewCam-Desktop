#include "ui/SettingsTab.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#ifdef _WIN32
#include "virtualcam/FilterRegistrar.h"
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Palette (mirrors MainWindow's Pal namespace)
// ─────────────────────────────────────────────────────────────────────────────
namespace SP {
    static constexpr const char *BgBase        = "#0D0D0F";
    static constexpr const char *BgElevated    = "#141417";
    static constexpr const char *Surface       = "#1C1C21";
    static constexpr const char *SurfaceRaised = "#232329";
    static constexpr const char *Accent        = "#3B82F6";
    static constexpr const char *AccentHover   = "#60A5FA";
    static constexpr const char *TextPrimary   = "rgba(255,255,255,0.92)";
    static constexpr const char *TextSecondary = "rgba(255,255,255,0.56)";
    static constexpr const char *TextMuted     = "rgba(255,255,255,0.34)";
    static constexpr const char *Success       = "#22C55E";
    static constexpr const char *Warning       = "#F59E0B";
    static constexpr const char *Error_        = "#EF4444";
    static constexpr const char *BorderSubtle  = "rgba(255,255,255,0.06)";
    static constexpr const char *BorderDefault = "rgba(255,255,255,0.10)";
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Section label: "VIRTUAL CAMERA" style
static QLabel *sectionLabel(const QString &text, QWidget *parent) {
    auto *lbl = new QLabel(text.toUpper(), parent);
    lbl->setStyleSheet(QString(
        "font-size: 10px;"
        "font-weight: 700;"
        "letter-spacing: 1.0px;"
        "color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(SP::TextMuted));
    return lbl;
}

// Card container with rounded corners and subtle border
static QFrame *card(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setStyleSheet(QString(
        "QFrame {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 12px;"
        "}"
        "QFrame * { background: transparent; border: none; }"
    ).arg(SP::BgElevated, SP::BorderSubtle));
    return f;
}

// A single 48px-tall settings row: label left, widget/value right
// Returns the row widget; pass content widget for right side (or nullptr for label-only right)
static QWidget *row(const QString &labelText, QWidget *rightWidget, QWidget *parent) {
    auto *w = new QWidget(parent);
    w->setFixedHeight(48);
    w->setStyleSheet("background: transparent;");

    auto *layout = new QHBoxLayout(w);
    layout->setContentsMargins(18, 0, 18, 0);
    layout->setSpacing(12);

    auto *lbl = new QLabel(labelText, w);
    lbl->setStyleSheet(QString(
        "font-size: 13px;"
        "font-weight: 500;"
        "color: %1;"
    ).arg(SP::TextPrimary));

    layout->addWidget(lbl);
    layout->addStretch();

    if (rightWidget) {
        rightWidget->setParent(w);
        layout->addWidget(rightWidget);
    }
    return w;
}

// Value-only right side (plain text)
static QLabel *valueLabel(const QString &text, QWidget *parent,
                           const char *color = SP::TextSecondary) {
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "font-size: 13px;"
        "color: %1;"
    ).arg(color));
    return lbl;
}

// Monospace value label (for IPs, paths, versions)
static QLabel *monoLabel(const QString &text, QWidget *parent,
                          const char *color = SP::TextMuted) {
    auto *lbl = new QLabel(text, parent);
    lbl->setStyleSheet(QString(
        "font-family: 'JetBrains Mono', 'Fira Code', monospace;"
        "font-size: 12px;"
        "color: %1;"
    ).arg(color));
    return lbl;
}

// Thin 1px horizontal divider inside a card
static QFrame *divider(QWidget *parent) {
    auto *f = new QFrame(parent);
    f->setFrameShape(QFrame::HLine);
    f->setFixedHeight(1);
    f->setStyleSheet(QString(
        "border: none;"
        "border-top: 1px solid %1;"
        "background: transparent;"
    ).arg(SP::BorderSubtle));
    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// SettingsTab
// ─────────────────────────────────────────────────────────────────────────────
SettingsTab::SettingsTab(Settings *settings, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("settingsPage");
    setStyleSheet(QString("background: %1;").arg(SP::BgBase));

    // ── Scroll area ───────────────────────────────────────────────────────────
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "  background: rgba(255,255,255,0.10); border-radius: 3px; min-height: 24px;"
        "}"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.20); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(SP::BgBase));

    auto *content = new QWidget();
    content->setStyleSheet(QString("background: %1;").arg(SP::BgBase));

    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(24, 24, 24, 40);
    layout->setSpacing(6);

    // ── APPEARANCE ────────────────────────────────────────────────────────────
    layout->addWidget(sectionLabel("Appearance", content));
    layout->addSpacing(6);
    {
        auto *c = card(content);
        auto *cl = new QVBoxLayout(c);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);

        // Segmented theme picker ─────────────────────────────────────────────
        // Build a horizontal group of three toggle buttons: Auto | Dark | Light
        auto *pickerContainer = new QWidget(c);
        pickerContainer->setStyleSheet("background: transparent;");
        pickerContainer->setFixedHeight(48);
        auto *pickerLayout = new QHBoxLayout(pickerContainer);
        pickerLayout->setContentsMargins(18, 0, 18, 0);
        pickerLayout->setSpacing(12);

        auto *themeLabel = new QLabel("Theme", pickerContainer);
        themeLabel->setStyleSheet(QString(
            "font-size: 13px; font-weight: 500; color: %1;"
        ).arg(SP::TextPrimary));
        pickerLayout->addWidget(themeLabel);
        pickerLayout->addStretch();

        // Pill container for the three buttons
        auto *pill = new QFrame(pickerContainer);
        pill->setStyleSheet(QString(
            "QFrame {"
            "  background: %1;"
            "  border: 1px solid %2;"
            "  border-radius: 8px;"
            "}"
        ).arg(SP::Surface, SP::BorderSubtle));
        auto *pillLayout = new QHBoxLayout(pill);
        pillLayout->setContentsMargins(3, 3, 3, 3);
        pillLayout->setSpacing(2);

        const QStringList labels = { "Auto", "Dark", "Light" };
        for (int i = 0; i < 3; ++i) {
            m_themeButtons[i] = new QPushButton(labels[i], pill);
            m_themeButtons[i]->setFixedHeight(26);
            m_themeButtons[i]->setCursor(Qt::PointingHandCursor);
            m_themeButtons[i]->setCheckable(false);
            updateThemeButton(i, i == 1); // default: Dark selected

            connect(m_themeButtons[i], &QPushButton::clicked, this, [this, i]() {
                for (int j = 0; j < 3; ++j)
                    updateThemeButton(j, j == i);
                emit themeChanged(i);
            });
            pillLayout->addWidget(m_themeButtons[i]);
        }

        pickerLayout->addWidget(pill);
        cl->addWidget(pickerContainer);
        layout->addWidget(c);
    }

    layout->addSpacing(20);

    // ── VIRTUAL CAMERA ────────────────────────────────────────────────────────
    layout->addWidget(sectionLabel("Virtual Camera", content));
    layout->addSpacing(6);
    {
        auto *c = card(content);
        auto *cl = new QVBoxLayout(c);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);

#ifdef __linux__
        cl->addWidget(row("Backend",  valueLabel("v4l2loopback", c), c));
        cl->addWidget(divider(c));
        cl->addWidget(row("Device",   monoLabel("/dev/video10", c), c));
#elif defined(_WIN32)
        cl->addWidget(row("Backend",  valueLabel("DirectShow", c), c));
        cl->addWidget(divider(c));

        // Filter status row — value label is stored for later updates
        {
            m_filterStatusLabel = new QLabel("Checking...", c);
            m_filterStatusLabel->setStyleSheet(QString(
                "font-size: 12px; font-weight: 600; color: %1;"
            ).arg(SP::TextMuted));
            cl->addWidget(row("Filter Status", m_filterStatusLabel, c));
        }
        cl->addWidget(divider(c));

        // Filter DLL path
        {
            m_filterPathLabel = new QLabel("", c);
            m_filterPathLabel->setStyleSheet(QString(
                "font-family: 'JetBrains Mono','Fira Code',monospace;"
                "font-size: 11px; color: %1;"
            ).arg(SP::TextMuted));
            m_filterPathLabel->setWordWrap(false);
            cl->addWidget(row("Filter DLL", m_filterPathLabel, c));
        }
        cl->addWidget(divider(c));

        // Install / Uninstall buttons
        {
            auto *btnRow = new QWidget(c);
            btnRow->setFixedHeight(56);
            btnRow->setStyleSheet("background: transparent;");
            auto *btnLayout = new QHBoxLayout(btnRow);
            btnLayout->setContentsMargins(18, 10, 18, 10);
            btnLayout->setSpacing(8);
            btnLayout->addStretch();

            m_installBtn = new QPushButton("Install Filter", btnRow);
            m_installBtn->setFixedHeight(34);
            m_installBtn->setCursor(Qt::PointingHandCursor);
            m_installBtn->setStyleSheet(QString(
                "QPushButton {"
                "  background: %1;"
                "  color: white;"
                "  border: none;"
                "  border-radius: 8px;"
                "  font-size: 12px;"
                "  font-weight: 600;"
                "  padding: 0 16px;"
                "}"
                "QPushButton:hover { background: %2; }"
                "QPushButton:disabled { background: rgba(59,130,246,0.3); }"
            ).arg(SP::Accent, SP::AccentHover));

            m_uninstallBtn = new QPushButton("Uninstall", btnRow);
            m_uninstallBtn->setFixedHeight(34);
            m_uninstallBtn->setCursor(Qt::PointingHandCursor);
            m_uninstallBtn->setStyleSheet(
                "QPushButton {"
                "  background: rgba(239,68,68,0.10);"
                "  color: #EF4444;"
                "  border: 1px solid rgba(239,68,68,0.25);"
                "  border-radius: 8px;"
                "  font-size: 12px;"
                "  font-weight: 600;"
                "  padding: 0 16px;"
                "}"
                "QPushButton:hover {"
                "  background: rgba(239,68,68,0.18);"
                "  border: 1px solid rgba(239,68,68,0.40);"
                "}"
                "QPushButton:disabled { opacity: 0.4; }"
            );

            connect(m_installBtn, &QPushButton::clicked, this, [this]() {
                m_installBtn->setEnabled(false);
                m_filterStatusLabel->setText("Registering...");
                FilterRegistrar::registerFilter();
                updateFilterStatus();
                m_installBtn->setEnabled(true);
            });
            connect(m_uninstallBtn, &QPushButton::clicked, this, [this]() {
                m_uninstallBtn->setEnabled(false);
                m_filterStatusLabel->setText("Unregistering...");
                FilterRegistrar::unregisterFilter();
                updateFilterStatus();
                m_uninstallBtn->setEnabled(true);
            });

            btnLayout->addWidget(m_installBtn);
            btnLayout->addWidget(m_uninstallBtn);
            cl->addWidget(btnRow);
        }
#endif
        layout->addWidget(c);
    }

    layout->addSpacing(20);

    // ── CONNECTION ────────────────────────────────────────────────────────────
    layout->addWidget(sectionLabel("Connection", content));
    layout->addSpacing(6);
    {
        auto *c = card(content);
        auto *cl = new QVBoxLayout(c);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);

        cl->addWidget(row("Default Port",
            monoLabel(QString::number(settings->port()), c), c));
        cl->addWidget(divider(c));
        cl->addWidget(row("Discovery",
            valueLabel("UDP Broadcast  :8081", c), c));

        layout->addWidget(c);
    }

    layout->addSpacing(20);

    // ── ABOUT ─────────────────────────────────────────────────────────────────
    layout->addWidget(sectionLabel("About", content));
    layout->addSpacing(6);
    {
        auto *c = card(content);
        auto *cl = new QVBoxLayout(c);
        cl->setContentsMargins(0, 0, 0, 0);
        cl->setSpacing(0);

        cl->addWidget(row("ViewCam",
            valueLabel("0.1.0", c, SP::TextSecondary), c));
        cl->addWidget(divider(c));
        cl->addWidget(row("Protocol",
            valueLabel("VCAM v1 · MJPEG", c, SP::TextSecondary), c));
        cl->addWidget(divider(c));
        cl->addWidget(row("Qt",
            monoLabel(qVersion(), c), c));

        layout->addWidget(c);
    }

    layout->addStretch();

    scrollArea->setWidget(content);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scrollArea);
}

// ─────────────────────────────────────────────────────────────────────────────
// Update segmented theme button appearance
// ─────────────────────────────────────────────────────────────────────────────
void SettingsTab::updateThemeButton(int index, bool active) {
    if (!m_themeButtons[index]) return;
    if (active) {
        m_themeButtons[index]->setStyleSheet(QString(
            "QPushButton {"
            "  background: %1;"
            "  color: %2;"
            "  border: 1px solid %3;"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "  font-weight: 600;"
            "  padding: 0 14px;"
            "}"
        ).arg(SP::SurfaceRaised, SP::TextPrimary, SP::BorderDefault));
    } else {
        m_themeButtons[index]->setStyleSheet(QString(
            "QPushButton {"
            "  background: transparent;"
            "  color: %1;"
            "  border: none;"
            "  border-radius: 6px;"
            "  font-size: 12px;"
            "  font-weight: 400;"
            "  padding: 0 14px;"
            "}"
            "QPushButton:hover { background: rgba(255,255,255,0.04); }"
        ).arg(SP::TextSecondary));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────
void SettingsTab::setThemeIndex(int index) {
    for (int i = 0; i < 3; ++i)
        updateThemeButton(i, i == index);
}

#ifdef _WIN32
void SettingsTab::updateFilterStatus() {
    auto status = FilterRegistrar::checkStatus();
    QString text = FilterRegistrar::statusText(status);

    switch (status) {
    case FilterRegistrar::Status::Registered:
        m_filterStatusLabel->setStyleSheet(QString(
            "font-size: 12px; font-weight: 600; color: %1;").arg(SP::Success));
        break;
    case FilterRegistrar::Status::RegisteredStale:
        m_filterStatusLabel->setStyleSheet(QString(
            "font-size: 12px; font-weight: 600; color: %1;").arg(SP::Warning));
        break;
    case FilterRegistrar::Status::NotRegistered:
        m_filterStatusLabel->setStyleSheet(QString(
            "font-size: 12px; font-weight: 600; color: %1;").arg(SP::Error_));
        break;
    }
    m_filterStatusLabel->setText(text);

    QString regPath = FilterRegistrar::registeredDllPath();
    m_filterPathLabel->setText(regPath.isEmpty()
        ? FilterRegistrar::expectedDllPath() : regPath);

    bool installed = (status == FilterRegistrar::Status::Registered);
    m_installBtn->setEnabled(!installed);
    m_uninstallBtn->setEnabled(status != FilterRegistrar::Status::NotRegistered);
}
#endif
