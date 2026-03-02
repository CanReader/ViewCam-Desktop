#include "ui/MainWindow.h"
#include "ui/CameraPreviewWidget.h"
#include "ui/ConnectionPanel.h"
#include "ui/AudioTab.h"
#include "ui/SettingsTab.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTabBar>
#include <QLabel>
#include <QFrame>
#include <QStatusBar>
#include <QGraphicsDropShadowEffect>
#include <QFont>
#include <QPalette>

static const char *GLOBAL_STYLE = R"(
    * {
        font-family: "Segoe UI", "SF Pro Display", "Helvetica Neue", "Noto Sans", sans-serif;
    }
    QMainWindow {
        background-color: #ECEEF1;
        color: #1A1A2E;
    }
    QWidget {
        background-color: #ECEEF1;
        color: #1A1A2E;
    }

    /* ── Tab widget ── */
    QTabWidget::pane {
        border: none;
        background-color: #ECEEF1;
        margin-top: -1px;
    }
    QTabBar {
        background: #FFFFFF;
        border-bottom: 1px solid #E0E3E8;
    }
    QTabBar::tab {
        background: transparent;
        color: #8B90A0;
        padding: 12px 28px;
        font-size: 13px;
        font-weight: 600;
        border: none;
        border-bottom: 3px solid transparent;
        margin-bottom: -1px;
    }
    QTabBar::tab:selected {
        color: #00897B;
        border-bottom: 3px solid #00897B;
    }
    QTabBar::tab:hover:!selected {
        color: #3D4055;
        background: #F5F6F8;
    }

    /* ── Global inputs ── */
    QLabel {
        color: #3D4055;
    }
    QComboBox, QLineEdit, QSpinBox {
        color: #1A1A2E;
        background: #F5F6F8;
        border: 1px solid #DDE0E6;
        border-radius: 6px;
        padding: 8px 10px;
        font-size: 13px;
        selection-background-color: #00897B;
        selection-color: #FFFFFF;
    }
    QComboBox:hover, QLineEdit:hover, QSpinBox:hover {
        border: 1px solid #B8BCC8;
    }
    QComboBox:focus, QLineEdit:focus, QSpinBox:focus {
        border: 2px solid #00897B;
        padding: 7px 9px;
    }
    QComboBox::drop-down {
        border: none;
        padding-right: 8px;
    }
    QComboBox QAbstractItemView {
        background: #FFFFFF;
        color: #1A1A2E;
        border: 1px solid #DDE0E6;
        border-radius: 6px;
        padding: 4px;
        selection-background-color: #E0F2F1;
        selection-color: #00695C;
    }
    QSpinBox::up-button, QSpinBox::down-button {
        border: none;
        background: transparent;
    }

    /* ── Status bar ── */
    QStatusBar {
        background-color: #FFFFFF;
        border-top: 1px solid #E0E3E8;
        color: #8B90A0;
        font-size: 12px;
        padding: 4px 12px;
        min-height: 28px;
    }
    QStatusBar QLabel {
        color: #8B90A0;
        font-size: 12px;
    }

    /* ── Scrollbar ── */
    QScrollBar:vertical {
        background: transparent;
        width: 8px;
        margin: 0;
    }
    QScrollBar::handle:vertical {
        background: #C8CCD4;
        border-radius: 4px;
        min-height: 30px;
    }
    QScrollBar::handle:vertical:hover {
        background: #A0A5B2;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0;
    }
)";

MainWindow::MainWindow(Settings *settings, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("ViewCam"));
    resize(900, 680);
    setStyleSheet(GLOBAL_STYLE);

    auto *central = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Header bar ──
    auto *headerBar = new QFrame(central);
    headerBar->setFixedHeight(56);
    headerBar->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #00897B, stop:1 #00796B);"
        "}"
        "QLabel { background: transparent; }"
    );
    auto *headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(20, 0, 20, 0);

    auto *titleLabel = new QLabel(tr("ViewCam"), headerBar);
    titleLabel->setStyleSheet(
        "font-size: 20px; font-weight: 700; color: white; letter-spacing: 0.5px;"
    );
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    auto *versionLabel = new QLabel(tr("v0.1.0"), headerBar);
    versionLabel->setStyleSheet(
        "font-size: 11px; color: rgba(255,255,255,0.6); font-weight: 500;"
    );
    headerLayout->addWidget(versionLabel);

    mainLayout->addWidget(headerBar);

    // ── Tab widget ──
    m_tabWidget = new QTabWidget(central);
    m_tabWidget->setDocumentMode(true);
    m_tabWidget->tabBar()->setExpanding(false);

    // Player tab
    auto *playerTab = new QWidget();
    playerTab->setStyleSheet("background-color: #ECEEF1;");
    auto *playerLayout = new QVBoxLayout(playerTab);
    playerLayout->setContentsMargins(20, 20, 20, 20);
    playerLayout->setSpacing(16);

    // Preview card with shadow
    auto *previewCard = new QFrame(playerTab);
    previewCard->setObjectName("previewCard");
    previewCard->setStyleSheet(
        "#previewCard {"
        "  background-color: #0D0D0D;"
        "  border: none;"
        "  border-radius: 12px;"
        "}"
    );
    auto *previewShadow = new QGraphicsDropShadowEffect(previewCard);
    previewShadow->setBlurRadius(24);
    previewShadow->setColor(QColor(0, 0, 0, 50));
    previewShadow->setOffset(0, 4);
    previewCard->setGraphicsEffect(previewShadow);

    auto *previewCardLayout = new QVBoxLayout(previewCard);
    previewCardLayout->setContentsMargins(3, 3, 3, 3);

    m_preview = new CameraPreviewWidget(previewCard);
    previewCardLayout->addWidget(m_preview);

    playerLayout->addWidget(previewCard, 1);

    // Connection area
    m_connectionPanel = new ConnectionPanel(playerTab);
    playerLayout->addWidget(m_connectionPanel, 0);

    m_tabWidget->addTab(playerTab, tr("  Player  "));

    // Audio tab
    m_audioTab = new AudioTab();
    m_tabWidget->addTab(m_audioTab, tr("  Audio  "));

    // Settings tab
    m_settingsTab = new SettingsTab(settings);
    m_tabWidget->addTab(m_settingsTab, tr("  Settings  "));

    mainLayout->addWidget(m_tabWidget, 1);

    setCentralWidget(central);

    // ── Status bar ──
    auto *sbar = statusBar();
    m_statusBarLabel = new QLabel(tr("Disconnected"), this);
    m_fpsBarLabel = new QLabel("", this);
    sbar->addWidget(m_statusBarLabel, 1);
    sbar->addPermanentWidget(m_fpsBarLabel);

    VC_DEBUG("MainWindow created with tabbed layout (900x680)");
}

void MainWindow::setStatusText(const QString &text) {
    m_statusBarLabel->setText(text);
}

void MainWindow::setFpsText(const QString &text) {
    m_fpsBarLabel->setText(text);
}
