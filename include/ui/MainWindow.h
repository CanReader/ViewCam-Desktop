#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class QLabel;
class QPushButton;
class QFrame;
class QPropertyAnimation;
class QGraphicsOpacityEffect;
class CameraPreviewWidget;
class ConnectionPanel;
class SettingsTab;
class AudioTab;
class Settings;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Settings *settings, QWidget *parent = nullptr);

    CameraPreviewWidget *previewWidget() const { return m_preview; }
    ConnectionPanel     *connectionPanel() const { return m_connectionPanel; }
    SettingsTab         *settingsTab() const { return m_settingsTab; }

    void setStatusText(const QString &text);
    void setFpsText(const QString &text);

    // New navigation API
    void showCameraScreen(const QString &deviceName);
    void showDevicesScreen();
    void setVirtualCamStatus(bool active, bool error = false);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSettingsRequested();
    void onBackFromSettings();

private:
    void buildDevicesScreen(QWidget *parent);
    void buildCameraScreen(QWidget *parent);
    void buildSettingsPage(QWidget *parent);
    void applyGlobalStyle();
    void fadeToIndex(int index);

    // Screens container
    QStackedWidget *m_stack = nullptr;

    // Devices screen (index 0) widgets
    QWidget     *m_devicesScreen   = nullptr;
    QLabel      *m_vcamStatusDot   = nullptr;
    QLabel      *m_vcamStatusLabel = nullptr;

    // Camera screen (index 1) widgets
    QWidget             *m_cameraScreen    = nullptr;
    CameraPreviewWidget *m_preview         = nullptr;
    QLabel              *m_deviceNameLabel = nullptr;
    QLabel              *m_fpsChip         = nullptr;
    QLabel              *m_resChip         = nullptr;
    QLabel              *m_codecChip       = nullptr;
    QLabel              *m_camVcamPill     = nullptr;

    // Settings screen (index 2)
    QWidget     *m_settingsPage = nullptr;
    SettingsTab *m_settingsTab  = nullptr;

    // ConnectionPanel lives inside devices screen
    ConnectionPanel *m_connectionPanel = nullptr;

    // Unused legacy members kept to avoid breaking anything that forward-references them
    AudioTab *m_audioTab = nullptr;

    // Track which screen we came from before entering settings
    int m_previousScreenIndex = 0;

    // Settings* stored so buildSettingsPage can access it
    Settings *m_settingsPtr = nullptr;

    // Fade overlay — a solid-color widget that sits on top of everything.
    // Animating ITS opacity avoids touching CameraPreviewWidget's painter.
    QWidget                *m_fadeOverlay  = nullptr;
    QGraphicsOpacityEffect *m_fadeEffect   = nullptr;
    QPropertyAnimation     *m_fadeAnim     = nullptr;
};
