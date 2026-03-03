#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class QPushButton;
class QLabel;
class QFrame;
class CameraPreviewWidget;
class ConnectionPanel;
class AudioTab;
class SettingsTab;
class Settings;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(Settings *settings, QWidget *parent = nullptr);

    CameraPreviewWidget *previewWidget() const { return m_preview; }
    ConnectionPanel *connectionPanel() const { return m_connectionPanel; }

    void setStatusText(const QString &text);
    void setFpsText(const QString &text);

private:
    void applyTheme(bool dark);
    QPushButton *createNavButton(const QString &text, int index);

    bool m_isDark = true;

    // Sidebar
    QWidget *m_sidebar;
    QFrame *m_sidebarBrand;
    QPushButton *m_navButtons[3];
    int m_activeNav = 0;

    // Content
    QStackedWidget *m_stack;
    CameraPreviewWidget *m_preview;
    ConnectionPanel *m_connectionPanel;
    AudioTab *m_audioTab;
    SettingsTab *m_settingsTab;

    // Info bar
    QLabel *m_vcamChip;
    QLabel *m_resChip;
    QLabel *m_codecChip;
    QLabel *m_statusBarLabel;
    QLabel *m_fpsBarLabel;

    // Theme toggle
    QPushButton *m_darkBtn;
    QPushButton *m_lightBtn;
};
