#pragma once

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class ConnectionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    void addDevice(const QString &name, const QString &host, int port);
    void setConnected(bool connected);
    void setFps(int fps);

signals:
    void connectRequested(const QString &host, int port);
    void disconnectRequested();

private slots:
    void onConnectClicked();
    void onDeviceSelected(int index);

private:
    QComboBox *m_deviceCombo;
    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QPushButton *m_connectBtn;
    QLabel *m_fpsLabel;
    QLabel *m_statusLabel;
    QLabel *m_deviceDot;
    QLabel *m_deviceNameLabel;
    bool m_connected = false;
};
