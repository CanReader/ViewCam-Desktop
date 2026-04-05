#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>

class QVBoxLayout;
class QLabel;
class QFrame;

class ConnectionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ConnectionPanel(QWidget *parent = nullptr);

    // Public API — must stay unchanged for Application.cpp
    void addDevice(const QString &name, const QString &host, int port);
    void setConnected(bool connected);
    void setFps(int fps);
    void showSessionLimitMessage();

    // Called by MainWindow's back button to trigger a disconnect
    void triggerDisconnect();

signals:
    void connectRequested(const QString &host, int port);
    void disconnectRequested();
    // NEW: emitted when a discovered-device card is clicked
    void deviceCardClicked(const QString &name, const QString &host, int port);

private slots:
    void onManualConnectClicked();

private:
    // Cards section
    QWidget     *m_cardsContainer   = nullptr;
    QVBoxLayout *m_cardsLayout      = nullptr;
    QWidget     *m_emptyState       = nullptr;

    // Manual connect
    QLineEdit   *m_hostEdit         = nullptr;
    QPushButton *m_connectBtn       = nullptr;

    // State
    bool         m_connected        = false;
    QString      m_connectedHost;
    int          m_connectedPort    = 0;

    // Card tracking — avoid duplicates
    struct DeviceEntry {
        QString name;
        QString host;
        int     port;
    };
    QList<DeviceEntry> m_entries;

    void updateEmptyState();
    QWidget *createDeviceCard(const QString &name, const QString &host, int port);
};
