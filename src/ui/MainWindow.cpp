/*
 * MainWindow.cpp
 *
 * Two-screen layout:
 *   index 0  — Devices screen  (ConnectionPanel + header bar + status bar)
 *   index 1  — Camera screen   (full-window preview + overlay bars)
 *   index 2  — Settings page   (SettingsTab wrapped in a back-navigable page)
 *
 * Transitions use QGraphicsOpacityEffect + QPropertyAnimation for cross-fades.
 */

#include "ui/MainWindow.h"
#include "ui/CameraPreviewWidget.h"
#include "ui/ConnectionPanel.h"
#include "ui/AudioTab.h"
#include "ui/SettingsTab.h"
#include "core/Logger.h"
#include "core/Settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QFont>
#include <QSizePolicy>
#include <QTimer>
#include <QResizeEvent>
#include <QEvent>

// ─────────────────────────────────────────────────────────────────────────────
// Color palette constants
// ─────────────────────────────────────────────────────────────────────────────
namespace Pal {
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
    static constexpr const char *Error_        = "#EF4444";
    static constexpr const char *Warning       = "#F59E0B";
    static constexpr const char *BorderSubtle  = "rgba(255,255,255,0.06)";
    static constexpr const char *BorderDefault = "rgba(255,255,255,0.10)";
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────
static QLabel *makeChip(const QString &text, const QString &bgColor,
                        const QString &fgColor, const QString &borderColor,
                        QWidget *parent)
{
    auto *chip = new QLabel(text, parent);
    chip->setStyleSheet(QString(
        "QLabel {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 10px;"
        "  padding: 3px 10px;"
        "  font-size: 11px;"
        "  font-weight: 600;"
        "}"
    ).arg(bgColor, fgColor, borderColor));
    chip->setFixedHeight(22);
    return chip;
}

// ─────────────────────────────────────────────────────────────────────────────
// MainWindow constructor
// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(Settings *settings, QWidget *parent)
    : QMainWindow(parent)
    , m_settingsPtr(settings)
{
    setWindowTitle("ViewCam");
    resize(960, 640);
    setMinimumSize(640, 480);

    // Apply global style to main window and all child widgets
    applyGlobalStyle();

    // Root: a stacked widget that holds all three screens
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("rootStack");
    setCentralWidget(m_stack);

    // ── Screen 0: Devices ────────────────────────────────────────────────────
    m_devicesScreen = new QWidget();
    m_devicesScreen->setObjectName("devicesScreen");
    buildDevicesScreen(m_devicesScreen);
    m_stack->addWidget(m_devicesScreen);   // index 0

    // ── Screen 1: Camera ─────────────────────────────────────────────────────
    m_cameraScreen = new QWidget();
    m_cameraScreen->setObjectName("cameraScreen");
    buildCameraScreen(m_cameraScreen);
    m_stack->addWidget(m_cameraScreen);    // index 1

    // ── Screen 2: Settings ───────────────────────────────────────────────────
    m_settingsPage = new QWidget();
    m_settingsPage->setObjectName("settingsPage");
    buildSettingsPage(m_settingsPage);
    m_stack->addWidget(m_settingsPage);    // index 2

    // Start on devices screen
    m_stack->setCurrentIndex(0);

    // Fade overlay: a solid bg-colored widget that covers the entire central widget.
    // We animate ITS opacity to create a fade-to-black-then-back-to-content transition
    // without ever attaching a QGraphicsOpacityEffect to m_stack or CameraPreviewWidget,
    // which would cause QPainter conflicts when video frames arrive at 30fps.
    auto *central = centralWidget();
    m_fadeOverlay = new QWidget(central);
    m_fadeOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_fadeOverlay->setStyleSheet(QString("background: %1;").arg(Pal::BgBase));
    m_fadeOverlay->raise();
    m_fadeOverlay->hide();

    m_fadeEffect = new QGraphicsOpacityEffect(m_fadeOverlay);
    m_fadeEffect->setOpacity(0.0);
    m_fadeOverlay->setGraphicsEffect(m_fadeEffect);

    m_fadeAnim = new QPropertyAnimation(m_fadeEffect, "opacity", this);
    m_fadeAnim->setDuration(200);

    // Install event filter on centralWidget so we can keep the overlay sized correctly
    centralWidget()->installEventFilter(this);

    VC_DEBUG("MainWindow created (two-screen layout, 960x640)");
}

// ─────────────────────────────────────────────────────────────────────────────
// Screen builders
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::buildDevicesScreen(QWidget *screen) {
    auto *rootLayout = new QVBoxLayout(screen);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Header bar (56px) ────────────────────────────────────────────────────
    auto *header = new QFrame(screen);
    header->setObjectName("devicesHeader");
    header->setFixedHeight(56);
    header->setStyleSheet(QString(
        "QFrame#devicesHeader {"
        "  background: %1;"
        "  border-bottom: 1px solid %2;"
        "}"
    ).arg(Pal::BgElevated, Pal::BorderSubtle));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 0, 20, 0);
    headerLayout->setSpacing(0);

    // Wordmark: "View" in accent, "Cam" in primary
    auto *wordmark = new QLabel(header);
    wordmark->setObjectName("wordmark");
    wordmark->setTextFormat(Qt::RichText);
    wordmark->setText(QString(
        "<span style='color:%1; font-size:18px; font-weight:800; letter-spacing:-0.5px;'>View</span>"
        "<span style='color:%2; font-size:18px; font-weight:800; letter-spacing:-0.5px;'>Cam</span>"
    ).arg(Pal::Accent, Pal::TextPrimary));
    wordmark->setStyleSheet("background: transparent; border: none;");

    // Settings gear button
    auto *settingsBtn = new QPushButton(screen);
    settingsBtn->setObjectName("settingsGear");
    settingsBtn->setFixedSize(36, 36);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    // Unicode gear: U+2699
    settingsBtn->setText(QString::fromUtf8("\xe2\x9a\x99"));
    settingsBtn->setStyleSheet(QString(
        "QPushButton#settingsGear {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-size: 18px;"
        "}"
        "QPushButton#settingsGear:hover {"
        "  background: rgba(255,255,255,0.06);"
        "  color: %2;"
        "}"
    ).arg(Pal::TextMuted, Pal::TextPrimary));

    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsRequested);

    headerLayout->addWidget(wordmark);
    headerLayout->addStretch();
    headerLayout->addWidget(settingsBtn);
    rootLayout->addWidget(header);

    // ── Content area (centered, 480px column) ────────────────────────────────
    auto *contentArea = new QWidget(screen);
    contentArea->setObjectName("devicesContent");
    contentArea->setStyleSheet(QString("background: %1;").arg(Pal::BgBase));

    auto *centeredLayout = new QHBoxLayout(contentArea);
    centeredLayout->setContentsMargins(0, 0, 0, 0);
    centeredLayout->setSpacing(0);

    // Center column container (max 480px, horizontally centered)
    auto *column = new QWidget(contentArea);
    column->setObjectName("devicesColumn");
    column->setMaximumWidth(480);
    column->setStyleSheet("background: transparent;");

    auto *columnLayout = new QVBoxLayout(column);
    columnLayout->setContentsMargins(0, 28, 0, 0);
    columnLayout->setSpacing(0);

    // ConnectionPanel fills the column
    m_connectionPanel = new ConnectionPanel(column);
    m_connectionPanel->setObjectName("connectionPanel");
    m_connectionPanel->setStyleSheet("background: transparent;");
    columnLayout->addWidget(m_connectionPanel, 1);

    centeredLayout->addStretch();
    centeredLayout->addWidget(column, 0, Qt::AlignTop);
    centeredLayout->addStretch();

    rootLayout->addWidget(contentArea, 1);

    // ── Status bar (28px) ────────────────────────────────────────────────────
    auto *statusBar = new QFrame(screen);
    statusBar->setObjectName("devicesStatusBar");
    statusBar->setFixedHeight(28);
    statusBar->setStyleSheet(QString(
        "QFrame#devicesStatusBar {"
        "  background: %1;"
        "  border-top: 1px solid %2;"
        "}"
    ).arg(Pal::BgElevated, Pal::BorderSubtle));

    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(16, 0, 16, 0);
    statusLayout->setSpacing(6);

    m_vcamStatusDot = new QLabel(statusBar);
    m_vcamStatusDot->setFixedSize(7, 7);
    m_vcamStatusDot->setObjectName("vcamDot");
    m_vcamStatusDot->setStyleSheet(QString(
        "background: %1; border-radius: 3px; border: none;"
    ).arg(Pal::TextMuted));

    m_vcamStatusLabel = new QLabel("Virtual camera inactive", statusBar);
    m_vcamStatusLabel->setObjectName("vcamStatusLabel");
    m_vcamStatusLabel->setStyleSheet(QString(
        "font-size: 11px; color: %1; background: transparent; border: none;"
    ).arg(Pal::TextMuted));

    statusLayout->addWidget(m_vcamStatusDot);
    statusLayout->addWidget(m_vcamStatusLabel);
    statusLayout->addStretch();

    rootLayout->addWidget(statusBar);
}

void MainWindow::buildCameraScreen(QWidget *screen) {
    // Use absolute positioning (overlay) — outer widget is a black full-window view
    screen->setStyleSheet("background: #000000;");

    // The preview widget fills the screen
    m_preview = new CameraPreviewWidget(screen);
    m_preview->setObjectName("cameraPreview");

    // ── Top overlay bar (52px) ───────────────────────────────────────────────
    auto *topBar = new QFrame(screen);
    topBar->setObjectName("camTopBar");
    topBar->setStyleSheet(
        "QFrame#camTopBar {"
        "  background: rgba(13,13,15,0.72);"
        "  border: none;"
        "}"
    );
    topBar->setFixedHeight(52);

    auto *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16, 0, 16, 0);
    topLayout->setSpacing(8);

    // "← Devices" back button
    auto *backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + "  Devices", topBar);
    backBtn->setObjectName("camBackBtn");
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(32);
    backBtn->setStyleSheet(QString(
        "QPushButton#camBackBtn {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  padding: 0 8px;"
        "}"
        "QPushButton#camBackBtn:hover {"
        "  color: %2;"
        "}"
    ).arg(Pal::Accent, Pal::AccentHover));

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        showDevicesScreen();
        // Notify Application that the user manually disconnected
        if (m_connectionPanel)
            m_connectionPanel->triggerDisconnect();
    });

    // Device name (centered)
    m_deviceNameLabel = new QLabel("", topBar);
    m_deviceNameLabel->setObjectName("camDeviceName");
    m_deviceNameLabel->setAlignment(Qt::AlignCenter);
    m_deviceNameLabel->setStyleSheet(QString(
        "font-size: 14px;"
        "font-weight: 600;"
        "color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(Pal::TextPrimary));

    // Top-right settings gear
    auto *camSettingsBtn = new QPushButton(topBar);
    camSettingsBtn->setObjectName("camSettingsBtn");
    camSettingsBtn->setFixedSize(32, 32);
    camSettingsBtn->setCursor(Qt::PointingHandCursor);
    camSettingsBtn->setText(QString::fromUtf8("\xe2\x9a\x99"));
    camSettingsBtn->setStyleSheet(QString(
        "QPushButton#camSettingsBtn {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 16px;"
        "  border-radius: 6px;"
        "}"
        "QPushButton#camSettingsBtn:hover {"
        "  background: rgba(255,255,255,0.08);"
        "  color: %2;"
        "}"
    ).arg(Pal::TextSecondary, Pal::TextPrimary));

    connect(camSettingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsRequested);

    topLayout->addWidget(backBtn);
    topLayout->addStretch();
    topLayout->addWidget(m_deviceNameLabel);
    topLayout->addStretch();
    topLayout->addWidget(camSettingsBtn);

    // ── Bottom overlay bar ───────────────────────────────────────────────────
    auto *bottomBar = new QFrame(screen);
    bottomBar->setObjectName("camBottomBar");
    bottomBar->setStyleSheet(
        "QFrame#camBottomBar {"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 transparent, stop:1 rgba(13,13,15,0.85));"
        "  border: none;"
        "}"
    );
    bottomBar->setFixedHeight(56);

    auto *bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(16, 0, 16, 0);
    bottomLayout->setSpacing(8);

    // Left chips: FPS | Resolution | MJPEG
    m_fpsChip = makeChip("-- FPS",
                          "rgba(59,130,246,0.10)",
                          Pal::Accent,
                          "rgba(59,130,246,0.25)",
                          bottomBar);
    m_fpsChip->setObjectName("fpsChip");

    m_resChip = makeChip("1280×720",
                          "rgba(255,255,255,0.05)",
                          Pal::TextSecondary,
                          Pal::BorderSubtle,
                          bottomBar);
    m_resChip->setObjectName("resChip");

    m_codecChip = makeChip("MJPEG",
                            "rgba(59,130,246,0.08)",
                            Pal::TextSecondary,
                            Pal::BorderDefault,
                            bottomBar);
    m_codecChip->setObjectName("codecChip");

    bottomLayout->addWidget(m_fpsChip);
    bottomLayout->addWidget(m_resChip);
    bottomLayout->addWidget(m_codecChip);
    bottomLayout->addStretch();

    // Right: virtual cam status pill + signal bars
    m_camVcamPill = new QLabel(QString::fromUtf8("\xe2\x97\x8f") + "  Virtual Cam", bottomBar);
    m_camVcamPill->setObjectName("camVcamPill");
    m_camVcamPill->setStyleSheet(QString(
        "background: rgba(34,197,94,0.10);"
        "color: %1;"
        "border: 1px solid rgba(34,197,94,0.30);"
        "border-radius: 10px;"
        "padding: 3px 10px;"
        "font-size: 11px;"
        "font-weight: 600;"
    ).arg(Pal::Success));
    m_camVcamPill->setFixedHeight(22);

    auto *signalBars = new QLabel("||||", bottomBar);
    signalBars->setStyleSheet(QString(
        "font-size: 11px;"
        "font-weight: 700;"
        "color: %1;"
        "background: transparent;"
        "border: none;"
        "letter-spacing: -1px;"
    ).arg(Pal::Success));

    bottomLayout->addWidget(m_camVcamPill);
    bottomLayout->addWidget(signalBars);

    // ── Use resizeEvent-driven absolute positioning ───────────────────────────
    // We position overlays after the screen's first show via an event filter approach.
    // For simplicity we use a layout hack: a QVBoxLayout with invisible spacer in the
    // middle occupies the full screen, and topBar/bottomBar get fixed heights.
    // This is compatible with QMainWindow resize events.

    auto *overlayLayout = new QVBoxLayout(screen);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(topBar);
    overlayLayout->addStretch(1);   // <-- preview sits behind via separate stacking
    overlayLayout->addWidget(bottomBar);

    // The preview must be a sibling that we push behind the layout's widgets.
    // Raise the overlay widgets above the preview.
    m_preview->lower();
    topBar->raise();
    bottomBar->raise();

    // The preview needs to match the screen geometry. We install an event filter.
    screen->installEventFilter(this);
    // NOTE: centralWidget() event filter is installed after setCentralWidget in the constructor.
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_cameraScreen && event->type() == QEvent::Resize) {
        // Keep the preview filling the camera screen
        if (m_preview)
            m_preview->setGeometry(0, 0, m_cameraScreen->width(), m_cameraScreen->height());
    }
    if (obj == centralWidget() && event->type() == QEvent::Resize) {
        // Keep the fade overlay covering the whole central widget
        if (m_fadeOverlay)
            m_fadeOverlay->setGeometry(centralWidget()->rect());
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::buildSettingsPage(QWidget *page) {
    page->setStyleSheet(QString("background: %1;").arg(Pal::BgBase));

    auto *rootLayout = new QVBoxLayout(page);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ── Back header ─────────────────────────────────────────────────────────
    auto *header = new QFrame(page);
    header->setObjectName("settingsHeader");
    header->setFixedHeight(56);
    header->setStyleSheet(QString(
        "QFrame#settingsHeader {"
        "  background: %1;"
        "  border-bottom: 1px solid %2;"
        "}"
    ).arg(Pal::BgElevated, Pal::BorderSubtle));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 24, 0);
    headerLayout->setSpacing(8);

    auto *backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90") + "  Back", header);
    backBtn->setObjectName("settingsBackBtn");
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(32);
    backBtn->setStyleSheet(QString(
        "QPushButton#settingsBackBtn {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 13px;"
        "  font-weight: 600;"
        "  padding: 0 8px;"
        "}"
        "QPushButton#settingsBackBtn:hover {"
        "  color: %2;"
        "}"
    ).arg(Pal::Accent, Pal::AccentHover));
    connect(backBtn, &QPushButton::clicked, this, &MainWindow::onBackFromSettings);

    auto *titleLabel = new QLabel("Settings", header);
    titleLabel->setStyleSheet(QString(
        "font-size: 15px;"
        "font-weight: 700;"
        "color: %1;"
        "background: transparent;"
        "border: none;"
    ).arg(Pal::TextPrimary));

    headerLayout->addWidget(backBtn);
    headerLayout->addStretch();
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    // Spacer to keep title visually centered (matches back btn width ~ 72px)
    auto *spacer = new QWidget(header);
    spacer->setFixedWidth(72);
    spacer->setStyleSheet("background: transparent;");
    headerLayout->addWidget(spacer);

    rootLayout->addWidget(header);

    // ── Settings content ─────────────────────────────────────────────────────
    // Wrap SettingsTab in a scroll area with the correct background
    auto *scrollArea = new QScrollArea(page);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(QString(
        "QScrollArea { background: %1; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background: rgba(255,255,255,0.12); border-radius: 3px; min-height: 24px; }"
        "QScrollBar::handle:vertical:hover { background: rgba(255,255,255,0.22); }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(Pal::BgBase));

    m_settingsTab = new SettingsTab(m_settingsPtr, scrollArea);
    m_settingsTab->setObjectName("settingsTabWidget");

    scrollArea->setWidget(m_settingsTab);
    rootLayout->addWidget(scrollArea, 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Global stylesheet
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::applyGlobalStyle() {
    setStyleSheet(QString(R"(
        * {
            font-family: "Inter", "Segoe UI", "SF Pro Display", sans-serif;
        }
        QMainWindow, #rootStack, #devicesScreen, #cameraScreen {
            background: %1;
        }
        /* ── SettingsTab inner widgets ─────────────────────────────────── */
        #settingsPage, #settingsTabWidget {
            background: %1;
        }
        #settingsCard {
            background: %2;
            border: 1px solid %3;
            border-radius: 12px;
        }
        #settingsCard * { background: transparent; }
        #sectionHeader {
            font-size: 10px;
            font-weight: 700;
            letter-spacing: 1px;
            color: %4;
            border: none;
            background: transparent;
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
            color: %6;
            border: none;
            background: transparent;
        }
        #settingsDivider {
            border: none;
            border-top: 1px solid %3;
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
            color: %6;
            background: transparent;
            border: none;
        }
        #comingSoonBadge {
            background: #F59E0B;
            color: white;
            border-radius: 10px;
            padding: 4px 12px;
            font-size: 11px;
            font-weight: 700;
        }
        /* ── QComboBox / QLineEdit / QSpinBox ──────────────────────────── */
        QComboBox, QLineEdit, QSpinBox {
            color: %5;
            background: %7;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 7px 10px;
            font-size: 12px;
            selection-background-color: %8;
            selection-color: #ffffff;
        }
        QComboBox:hover, QLineEdit:hover, QSpinBox:hover {
            border: 1px solid rgba(255,255,255,0.18);
        }
        QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
            border: 1px solid rgba(59,130,246,0.55);
        }
        QComboBox::drop-down { border: none; padding-right: 8px; }
        QComboBox QAbstractItemView {
            background: %2;
            color: %5;
            border: 1px solid %3;
            border-radius: 6px;
            padding: 4px;
            selection-background-color: %7;
            selection-color: %8;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            border: none; background: transparent;
        }
    )").arg(
        Pal::BgBase,          // %1
        Pal::Surface,         // %2
        Pal::BorderDefault,   // %3
        Pal::TextMuted,       // %4
        Pal::TextPrimary,     // %5
        Pal::TextSecondary,   // %6
        Pal::SurfaceRaised,   // %7
        Pal::Accent           // %8
    ));
}

// ─────────────────────────────────────────────────────────────────────────────
// Navigation
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::fadeToIndex(int index) {
    if (m_stack->currentIndex() == index)
        return;

    m_fadeAnim->stop();
    disconnect(m_fadeAnim, nullptr, nullptr, nullptr);

    // Size the overlay to cover the whole central widget
    m_fadeOverlay->setGeometry(centralWidget()->rect());
    m_fadeOverlay->show();
    m_fadeOverlay->raise();

    // Phase 1: fade overlay IN (content disappears behind it)
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();

    int targetIndex = index;
    connect(m_fadeAnim, &QPropertyAnimation::finished, this,
            [this, targetIndex]() {
        disconnect(m_fadeAnim, nullptr, nullptr, nullptr);

        // Switch the visible screen while the overlay is fully opaque
        m_stack->setCurrentIndex(targetIndex);

        // Phase 2: fade overlay OUT (new content revealed)
        m_fadeAnim->setStartValue(1.0);
        m_fadeAnim->setEndValue(0.0);
        m_fadeAnim->start();

        connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
            disconnect(m_fadeAnim, nullptr, nullptr, nullptr);
            m_fadeOverlay->hide();
        });
    });
}

void MainWindow::showCameraScreen(const QString &deviceName) {
    if (m_deviceNameLabel)
        m_deviceNameLabel->setText(deviceName);
    fadeToIndex(1);
}

void MainWindow::showDevicesScreen() {
    fadeToIndex(0);
}

void MainWindow::onSettingsRequested() {
    m_previousScreenIndex = m_stack->currentIndex();
    fadeToIndex(2);
}

void MainWindow::onBackFromSettings() {
    fadeToIndex(m_previousScreenIndex);
}

// ─────────────────────────────────────────────────────────────────────────────
// Status / FPS updates
// ─────────────────────────────────────────────────────────────────────────────
void MainWindow::setStatusText(const QString &text) {
    if (m_vcamStatusLabel)
        m_vcamStatusLabel->setText(text);
}

void MainWindow::setFpsText(const QString &text) {
    if (m_fpsChip) {
        m_fpsChip->setText(text.isEmpty() ? "-- FPS" : text);
    }
}

void MainWindow::setVirtualCamStatus(bool active, bool error) {
    if (!m_vcamStatusDot || !m_vcamStatusLabel || !m_camVcamPill)
        return;

    if (error) {
        m_vcamStatusDot->setStyleSheet(QString(
            "background: %1; border-radius: 3px; border: none;").arg(Pal::Error_));
        m_vcamStatusLabel->setText("Virtual camera error");
        m_vcamStatusLabel->setStyleSheet(QString(
            "font-size: 11px; color: %1; background: transparent; border: none;"
        ).arg(Pal::Error_));
        m_camVcamPill->setStyleSheet(
            "background: rgba(239,68,68,0.10);"
            "color: #EF4444;"
            "border: 1px solid rgba(239,68,68,0.30);"
            "border-radius: 10px;"
            "padding: 3px 10px;"
            "font-size: 11px;"
            "font-weight: 600;"
        );
        m_camVcamPill->setText(QString::fromUtf8("\xe2\x97\x8f") + "  Virtual Cam Error");
    } else if (active) {
        m_vcamStatusDot->setStyleSheet(QString(
            "background: %1; border-radius: 3px; border: none;").arg(Pal::Success));
        m_vcamStatusLabel->setText("Virtual camera active");
        m_vcamStatusLabel->setStyleSheet(QString(
            "font-size: 11px; color: %1; background: transparent; border: none;"
        ).arg(Pal::Success));
        m_camVcamPill->setStyleSheet(QString(
            "background: rgba(34,197,94,0.10);"
            "color: %1;"
            "border: 1px solid rgba(34,197,94,0.30);"
            "border-radius: 10px;"
            "padding: 3px 10px;"
            "font-size: 11px;"
            "font-weight: 600;"
        ).arg(Pal::Success));
        m_camVcamPill->setText(QString::fromUtf8("\xe2\x97\x8f") + "  Virtual Cam");
    } else {
        m_vcamStatusDot->setStyleSheet(
            "background: rgba(255,255,255,0.20); border-radius: 3px; border: none;");
        m_vcamStatusLabel->setText("Virtual camera inactive");
        m_vcamStatusLabel->setStyleSheet(QString(
            "font-size: 11px; color: %1; background: transparent; border: none;"
        ).arg(Pal::TextMuted));
        m_camVcamPill->setStyleSheet(
            "background: rgba(255,255,255,0.05);"
            "color: rgba(255,255,255,0.34);"
            "border: 1px solid rgba(255,255,255,0.08);"
            "border-radius: 10px;"
            "padding: 3px 10px;"
            "font-size: 11px;"
            "font-weight: 600;"
        );
        m_camVcamPill->setText(QString::fromUtf8("\xe2\x97\x8f") + "  Virtual Cam");
    }
}
