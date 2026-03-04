#include "ui/MainWindow.h"
#include "ui/CameraPreviewWidget.h"
#include "ui/ConnectionPanel.h"
#include "ui/AudioTab.h"
#include "ui/SettingsTab.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QGraphicsDropShadowEffect>
#include <QFont>
#include <QPalette>
#include <QGuiApplication>
#include <QStyleHints>

// ── Theme color structs ──
struct ThemeColors {
    QString bg0, bg1, bg2, bg3, bg4;
    QString bgInput;
    QString border1, border2, border3, borderAccent;
    QString text0, text1, text2, text3;
    QString cyan, gold, red, green;
    QString camBg, overlayBg;
    QString shadowSm, shadowMd;
};

static ThemeColors darkTheme() {
    return {
        "#060a14", "#0b1120", "#111827", "#172033", "#1e293b",
        "#0f172a",
        "rgba(255,255,255,0.04)", "rgba(255,255,255,0.08)", "rgba(255,255,255,0.12)", "rgba(79,195,247,0.2)",
        "#f1f5f9", "#cbd5e1", "#7a8baa", "#475569",
        "#4fc3f7", "#ffd54f", "#ef4444", "#22c55e",
        "#000000", "rgba(6,10,20,0.7)",
        "rgba(0,0,0,0.3)", "rgba(0,0,0,0.4)"
    };
}

static ThemeColors lightTheme() {
    return {
        "#f0f4f8", "#f8fafc", "#ffffff", "#f1f5f9", "#e8edf4",
        "#f1f5f9",
        "rgba(0,0,0,0.04)", "rgba(0,0,0,0.08)", "rgba(0,0,0,0.12)", "rgba(2,136,209,0.25)",
        "#0f172a", "#334155", "#64748b", "#94a3b8",
        "#0288d1", "#e6a700", "#d32f2f", "#2e7d32",
        "#0a0a0a", "rgba(0,0,0,0.55)",
        "rgba(0,0,0,0.06)", "rgba(0,0,0,0.08)"
    };
}

static QString buildGlobalStyle(const ThemeColors &c) {
    return QString(R"(
        * {
            font-family: "Segoe UI", "SF Pro Display", "Helvetica Neue", "Noto Sans", sans-serif;
        }
        QMainWindow {
            background-color: %1;
        }

        /* ── Scrollbar ── */
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: %2;
            border-radius: 4px;
            min-height: 30px;
        }
        QScrollBar::handle:vertical:hover {
            background: %3;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
    )").arg(c.bg0, c.text3, c.text2);
}

MainWindow::MainWindow(Settings *settings, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("ViewCam"));
    resize(960, 640);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Title bar ──
    auto *titleBar = new QFrame(central);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(38);
    auto *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(14, 0, 14, 0);
    titleBarLayout->setSpacing(7);

    // Window dots (decorative)
    for (auto color : {"#ef4444", "#fbbf24", "#22c55e"}) {
        auto *dot = new QFrame(titleBar);
        dot->setFixedSize(11, 11);
        dot->setStyleSheet(QString("background: %1; border-radius: 5px; border: none;").arg(color));
        titleBarLayout->addWidget(dot);
    }

    auto *titleText = new QLabel(tr("ViewCam"), titleBar);
    titleText->setObjectName("titleBarText");
    titleText->setAlignment(Qt::AlignCenter);
    titleBarLayout->addStretch();
    titleBarLayout->addWidget(titleText);
    titleBarLayout->addStretch();

    // Theme toggle
    auto *themeToggle = new QFrame(titleBar);
    themeToggle->setObjectName("themeToggle");
    auto *toggleLayout = new QHBoxLayout(themeToggle);
    toggleLayout->setContentsMargins(3, 3, 3, 3);
    toggleLayout->setSpacing(2);

    m_darkBtn = new QPushButton(QString::fromUtf8("\xF0\x9F\x8C\x99 Dark"), themeToggle);
    m_darkBtn->setObjectName("themeOptActive");
    m_darkBtn->setCursor(Qt::PointingHandCursor);
    m_darkBtn->setFixedHeight(26);

    m_lightBtn = new QPushButton(QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f Light"), themeToggle);
    m_lightBtn->setObjectName("themeOpt");
    m_lightBtn->setCursor(Qt::PointingHandCursor);
    m_lightBtn->setFixedHeight(26);

    toggleLayout->addWidget(m_darkBtn);
    toggleLayout->addWidget(m_lightBtn);
    titleBarLayout->addWidget(themeToggle);

    connect(m_darkBtn, &QPushButton::clicked, this, [this]() {
        applyTheme(true);
        m_settingsTab->setThemeIndex(1);
    });
    connect(m_lightBtn, &QPushButton::clicked, this, [this]() {
        applyTheme(false);
        m_settingsTab->setThemeIndex(2);
    });

    rootLayout->addWidget(titleBar);

    // ── Body: sidebar + main ──
    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    // ── Sidebar ──
    m_sidebar = new QWidget(body);
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(220);
    auto *sidebarLayout = new QVBoxLayout(m_sidebar);
    sidebarLayout->setContentsMargins(12, 16, 12, 16);
    sidebarLayout->setSpacing(6);

    // Brand
    auto *brandLabel = new QLabel(m_sidebar);
    brandLabel->setObjectName("sidebarBrand");
    brandLabel->setText(QString("<span style='color: %1;'>View</span>Cam").arg("#4fc3f7"));
    brandLabel->setTextFormat(Qt::RichText);
    sidebarLayout->addWidget(brandLabel);
    sidebarLayout->addSpacing(8);

    // Nav items
    QString navNames[] = {tr("Player"), tr("Audio"), tr("Settings")};
    for (int i = 0; i < 3; i++) {
        m_navButtons[i] = createNavButton(navNames[i], i);
        sidebarLayout->addWidget(m_navButtons[i]);
    }

    sidebarLayout->addStretch();

    // Connection panel (device card) at bottom of sidebar
    m_connectionPanel = new ConnectionPanel(m_sidebar);
    sidebarLayout->addWidget(m_connectionPanel);

    bodyLayout->addWidget(m_sidebar);

    // ── Main content ──
    auto *mainArea = new QWidget(body);
    mainArea->setObjectName("mainArea");
    auto *mainLayout = new QVBoxLayout(mainArea);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Stacked content
    m_stack = new QStackedWidget(mainArea);
    m_stack->setObjectName("contentStack");

    // Player page (camera + info bar)
    auto *playerPage = new QWidget();
    playerPage->setObjectName("playerPage");
    auto *playerLayout = new QVBoxLayout(playerPage);
    playerLayout->setContentsMargins(12, 12, 12, 12);
    playerLayout->setSpacing(8);

    // Camera viewport
    auto *camFrame = new QFrame(playerPage);
    camFrame->setObjectName("camFrame");
    auto *camLayout = new QVBoxLayout(camFrame);
    camLayout->setContentsMargins(0, 0, 0, 0);

    m_preview = new CameraPreviewWidget(camFrame);
    camLayout->addWidget(m_preview, 0, Qt::AlignCenter);
    playerLayout->addWidget(camFrame, 1);

    // Info bar
    auto *infoBar = new QFrame(playerPage);
    infoBar->setObjectName("infoBar");
    auto *infoLayout = new QHBoxLayout(infoBar);
    infoLayout->setContentsMargins(12, 10, 12, 10);
    infoLayout->setSpacing(8);

    m_vcamChip = new QLabel(QString::fromUtf8("\xe2\x97\x8f Virtual Camera"), infoBar);
    m_vcamChip->setObjectName("chipVcam");
    m_resChip = new QLabel("1280 \xc3\x97 720", infoBar);
    m_resChip->setObjectName("chipRes");
    m_codecChip = new QLabel("MJPEG", infoBar);
    m_codecChip->setObjectName("chipCodec");
    m_statusBarLabel = new QLabel(tr("Disconnected"), infoBar);
    m_statusBarLabel->setObjectName("statusLabel");
    m_fpsBarLabel = new QLabel("", infoBar);
    m_fpsBarLabel->setObjectName("fpsLabel");

    infoLayout->addWidget(m_vcamChip);
    infoLayout->addWidget(m_resChip);
    infoLayout->addWidget(m_codecChip);
    infoLayout->addStretch();
    infoLayout->addWidget(m_statusBarLabel);
    infoLayout->addWidget(m_fpsBarLabel);

    playerLayout->addWidget(infoBar);

    m_stack->addWidget(playerPage);

    // Audio tab
    m_audioTab = new AudioTab();
    m_stack->addWidget(m_audioTab);

    // Settings tab
    m_settingsTab = new SettingsTab(settings);
    m_stack->addWidget(m_settingsTab);

    // Wire settings theme combo to applyTheme
    connect(m_settingsTab, &SettingsTab::themeChanged, this, [this](int index) {
        if (index == 0) {
            // Auto — detect system
            bool systemDark = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
            auto bg = QGuiApplication::palette().color(QPalette::Window);
            systemDark = bg.lightness() < 128;
#endif
            applyTheme(systemDark);
        } else {
            applyTheme(index == 1); // 1=Dark, 2=Light
        }
        // Sync title bar toggle buttons
        m_darkBtn->setObjectName(m_isDark ? "themeOptActive" : "themeOpt");
        m_lightBtn->setObjectName(m_isDark ? "themeOpt" : "themeOptActive");
        m_darkBtn->style()->unpolish(m_darkBtn);
        m_darkBtn->style()->polish(m_darkBtn);
        m_lightBtn->style()->unpolish(m_lightBtn);
        m_lightBtn->style()->polish(m_lightBtn);
    });

    mainLayout->addWidget(m_stack);
    bodyLayout->addWidget(mainArea, 1);

    rootLayout->addWidget(body, 1);
    setCentralWidget(central);

    // Detect system dark mode
    bool systemDark = true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    auto bg = QGuiApplication::palette().color(QPalette::Window);
    systemDark = bg.lightness() < 128;
#endif

    applyTheme(systemDark);
    m_navButtons[0]->setProperty("active", true);

    VC_DEBUG("MainWindow created with sidebar layout (960x640)");
}

QPushButton *MainWindow::createNavButton(const QString &text, int index) {
    auto *btn = new QPushButton(text, m_sidebar);
    btn->setObjectName("navItem");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setProperty("active", index == 0);
    btn->setCheckable(false);

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        for (int i = 0; i < 3; i++) {
            m_navButtons[i]->setProperty("active", i == index);
            m_navButtons[i]->style()->unpolish(m_navButtons[i]);
            m_navButtons[i]->style()->polish(m_navButtons[i]);
        }
        m_activeNav = index;
        m_stack->setCurrentIndex(index);
    });

    return btn;
}

void MainWindow::applyTheme(bool dark) {
    m_isDark = dark;
    auto c = dark ? darkTheme() : lightTheme();

    // Update toggle button states
    m_darkBtn->setObjectName(dark ? "themeOptActive" : "themeOpt");
    m_lightBtn->setObjectName(dark ? "themeOpt" : "themeOptActive");

    QString style = buildGlobalStyle(c) + QString(R"(
        /* ── Title bar ── */
        #titleBar {
            background: %1;
            border-bottom: 1px solid %2;
        }
        #titleBarText {
            font-size: 12px;
            font-weight: 600;
            color: %3;
            background: transparent;
            border: none;
        }

        /* ── Theme toggle ── */
        #themeToggle {
            background: %4;
            border: 1px solid %2;
            border-radius: 15px;
        }
        #themeOpt {
            padding: 4px 12px;
            border-radius: 13px;
            font-size: 11px;
            font-weight: 600;
            color: %3;
            border: none;
            background: transparent;
        }
        #themeOptActive {
            padding: 4px 12px;
            border-radius: 13px;
            font-size: 11px;
            font-weight: 600;
            color: %5;
            border: none;
            background: %1;
        }

        /* ── Sidebar ── */
        #sidebar {
            background: %1;
            border-right: 1px solid %2;
        }
        #sidebarBrand {
            font-size: 17px;
            font-weight: 800;
            color: %5;
            padding: 4px 8px 10px;
            letter-spacing: -0.3px;
            background: transparent;
            border: none;
        }

        /* ── Nav items ── */
        #navItem {
            text-align: left;
            padding: 10px 12px;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 500;
            color: %3;
            border: 1px solid transparent;
            background: transparent;
        }
        #navItem:hover {
            background: %4;
            color: %6;
        }
        #navItem[active="true"] {
            background: %4;
            color: %7;
            border: 1px solid %8;
        }

        /* ── Main content ── */
        #mainArea, #contentStack, #playerPage {
            background: %9;
        }

        /* ── Camera frame ── */
        #camFrame {
            background: %10;
            border: 1px solid %2;
            border-radius: 14px;
        }

        /* ── Info bar ── */
        #infoBar {
            background: %1;
            border: 1px solid %2;
            border-radius: 10px;
        }

        /* ── Info chips ── */
        #chipVcam {
            color: %11;
            background: rgba(34,197,94,0.06);
            border: 1px solid rgba(34,197,94,0.2);
            border-radius: 10px;
            padding: 4px 10px;
            font-size: 10px;
            font-weight: 600;
        }
        #chipRes {
            color: %3;
            background: %4;
            border: 1px solid %2;
            border-radius: 10px;
            padding: 4px 10px;
            font-size: 10px;
            font-weight: 600;
        }
        #chipCodec {
            color: %7;
            background: rgba(79,195,247,0.06);
            border: 1px solid rgba(79,195,247,0.2);
            border-radius: 10px;
            padding: 4px 10px;
            font-size: 10px;
            font-weight: 600;
        }
        #statusLabel {
            color: %3;
            font-size: 11px;
            background: transparent;
            border: none;
        }
        #fpsLabel {
            color: %12;
            font-size: 11px;
            font-weight: 600;
            font-family: "JetBrains Mono", "Fira Code", monospace;
            background: transparent;
            border: none;
        }

        /* ── Connection panel ── */
        #connectionCard {
            background: %4;
            border: 1px solid %2;
            border-radius: 14px;
        }
        #connectionCard * {
            background: transparent;
        }
        #connectionCard QLabel {
            border: none;
            color: %6;
            font-size: 12px;
        }
        #connectionCard QLabel#cardHeader {
            font-size: 12px;
            font-weight: 700;
            color: %5;
        }
        #connectionCard QLabel#deviceDot {
            background: %11;
            border-radius: 4px;
        }
        #connectionCard QLabel#deviceName {
            font-size: 12px;
            font-weight: 700;
            color: %5;
        }
        #connectionCard QLabel#deviceAddr {
            font-family: "JetBrains Mono", "Fira Code", monospace;
            font-size: 10px;
            color: %13;
        }
        #connectionCard QLabel#statusLabel {
            font-size: 11px;
            color: %3;
        }
        #connectionCard QLabel#fpsLabel {
            font-size: 11px;
            color: %12;
            font-weight: 600;
        }

        QComboBox, QLineEdit, QSpinBox {
            color: %5;
            background: %14;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 7px 10px;
            font-size: 12px;
            selection-background-color: %7;
            selection-color: #ffffff;
        }
        QComboBox:hover, QLineEdit:hover, QSpinBox:hover {
            border: 1px solid %15;
        }
        QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
            border: 2px solid %7;
            padding: 6px 9px;
        }
        QComboBox::drop-down {
            border: none;
            padding-right: 8px;
        }
        QComboBox QAbstractItemView {
            background: %1;
            color: %5;
            border: 1px solid %2;
            border-radius: 6px;
            padding: 4px;
            selection-background-color: %4;
            selection-color: %7;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            border: none;
            background: transparent;
        }

        #connectBtn {
            background: %7;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 8px;
            font-size: 11px;
            font-weight: 700;
        }
        #connectBtn:hover {
            background: %16;
        }
        #disconnectBtn {
            background: rgba(239,68,68,0.08);
            color: %17;
            border: 1px solid rgba(239,68,68,0.2);
            border-radius: 6px;
            padding: 8px;
            font-size: 11px;
            font-weight: 700;
        }
        #disconnectBtn:hover {
            background: rgba(239,68,68,0.15);
            border: 1px solid rgba(239,68,68,0.35);
        }

        /* ── Settings & Audio tabs ── */
        #settingsPage, #audioPage {
            background: %9;
        }
        #settingsCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 12px;
        }
        #settingsCard * {
            background: transparent;
        }
        #audioCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 12px;
        }
        #audioCard * {
            background: transparent;
        }
        #sectionHeader {
            font-size: 11px;
            font-weight: 700;
            color: %13;
            letter-spacing: 1px;
            background: transparent;
            border: none;
        }
        #settingsLabel {
            font-size: 13px;
            color: %5;
            font-weight: 500;
            border: none;
            background: transparent;
        }
        #settingsValue {
            font-size: 13px;
            color: %3;
            border: none;
            background: transparent;
        }
        #settingsDivider {
            border: none;
            border-top: 1px solid %2;
            background: transparent;
        }
        #pageHeader {
            font-size: 20px;
            font-weight: 700;
            color: %5;
            background: transparent;
            border: none;
        }
        #pageSubtitle {
            font-size: 13px;
            color: %3;
            background: transparent;
            border: none;
        }
        #audioIcon {
            background: %4;
            border-radius: 24px;
            border: none;
        }
        #audioTitle {
            font-size: 15px;
            font-weight: 700;
            color: %5;
            border: none;
            background: transparent;
        }
        #audioDesc {
            font-size: 12px;
            color: %3;
            border: none;
            background: transparent;
        }
        #comingSoonBadge {
            background: #FF8F00;
            color: white;
            border-radius: 10px;
            padding: 4px 12px;
            font-size: 11px;
            font-weight: 700;
        }
    )").arg(
        c.bg2,          // %1  - sidebar/card bg
        c.border2,      // %2  - borders
        c.text2,        // %3  - secondary text
        c.bg3,          // %4  - nav hover/device card bg
        c.text0,        // %5  - primary text
        c.text1,        // %6  - text1
        c.cyan,         // %7  - accent cyan
        c.borderAccent, // %8  - accent border
        c.bg0           // %9  - main bg
    ).arg(
        c.camBg,        // %10 - camera bg
        c.green,        // %11 - green
        c.gold,         // %12 - gold
        c.text3,        // %13 - text3
        c.bgInput,      // %14 - input bg
        c.border3,      // %15 - hover border
        dark ? "#2a8ab8" : "#01579b", // %16 - btn hover
        c.red           // %17 - red
    );

    setStyleSheet(style);

    // Re-polish nav buttons
    for (int i = 0; i < 3; i++) {
        m_navButtons[i]->style()->unpolish(m_navButtons[i]);
        m_navButtons[i]->style()->polish(m_navButtons[i]);
    }
    m_darkBtn->style()->unpolish(m_darkBtn);
    m_darkBtn->style()->polish(m_darkBtn);
    m_lightBtn->style()->unpolish(m_lightBtn);
    m_lightBtn->style()->polish(m_lightBtn);

    // Update brand for light theme cyan
    auto *brand = m_sidebar->findChild<QLabel*>("sidebarBrand");
    if (brand) {
        brand->setText(QString("<span style='color: %1;'>View</span>Cam").arg(c.cyan));
    }
}

void MainWindow::setStatusText(const QString &text) {
    m_statusBarLabel->setText(text);
}

void MainWindow::setFpsText(const QString &text) {
    m_fpsBarLabel->setText(text);
}
