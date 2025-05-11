#pragma once

#include <QMainWindow>

class CameraPreviewWidget;
class ConnectionPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    CameraPreviewWidget *previewWidget() const { return m_preview; }
    ConnectionPanel *connectionPanel() const { return m_connectionPanel; }

private:
    CameraPreviewWidget *m_preview;
    ConnectionPanel *m_connectionPanel;
};
