#pragma once

#include <QMainWindow>

class QTabWidget;
class QLabel;
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
    QTabWidget *m_tabWidget;
    CameraPreviewWidget *m_preview;
    ConnectionPanel *m_connectionPanel;
    AudioTab *m_audioTab;
    SettingsTab *m_settingsTab;
    QLabel *m_statusBarLabel;
    QLabel *m_fpsBarLabel;
};
