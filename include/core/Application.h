#pragma once

#include <QObject>
#include <memory>

class Settings;
class StreamReceiver;
class DeviceDiscovery;
class FrameDecoder;
class MainWindow;
#ifdef __linux__
class V4L2LoopbackWriter;
#elif defined(_WIN32)
class DirectShowVirtualCam;
class FilterRegistrar;
#endif

class Application : public QObject {
    Q_OBJECT

public:
    explicit Application(QObject *parent = nullptr);
    ~Application();

    void init();

private slots:
    void onDeviceFound(const QString &name, const QString &host, int port);
    void onConnectRequested(const QString &host, int port);
    void onDisconnectRequested();

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<StreamReceiver> m_receiver;
    std::unique_ptr<DeviceDiscovery> m_discovery;
    std::unique_ptr<FrameDecoder> m_decoder;
    std::unique_ptr<MainWindow> m_window;
#ifdef __linux__
    std::unique_ptr<V4L2LoopbackWriter> m_vcamWriter;
#elif defined(_WIN32)
    std::unique_ptr<DirectShowVirtualCam> m_vcamWriter;
#endif

    // Track name of currently connected device for the camera screen title
    QString m_connectedDeviceName;
};
