#include "core/Application.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "network/StreamReceiver.h"
#include "network/DeviceDiscovery.h"
#include "network/FrameDecoder.h"
#include "ui/MainWindow.h"
#include "ui/CameraPreviewWidget.h"
#include "ui/ConnectionPanel.h"
#ifdef __linux__
#include "virtualcam/V4L2LoopbackWriter.h"
#endif

Application::Application(QObject *parent)
    : QObject(parent)
    , m_settings(std::make_unique<Settings>(this))
    , m_receiver(std::make_unique<StreamReceiver>(this))
    , m_discovery(std::make_unique<DeviceDiscovery>(this))
    , m_decoder(std::make_unique<FrameDecoder>(this))
    , m_window(std::make_unique<MainWindow>())
#ifdef __linux__
    , m_vcamWriter(std::make_unique<V4L2LoopbackWriter>(this))
#endif
{
    VC_DEBUG("Application created");
}

Application::~Application() {
    VC_DEBUG("Application destroyed");
}

void Application::init() {
    VC_INFO("Initializing application");

    // Wire up discovery -> connection panel
    connect(m_discovery.get(), &DeviceDiscovery::deviceFound,
            this, &Application::onDeviceFound);

    // Wire up connection panel -> receiver
    connect(m_window->connectionPanel(), &ConnectionPanel::connectRequested,
            this, &Application::onConnectRequested);
    connect(m_window->connectionPanel(), &ConnectionPanel::disconnectRequested,
            this, &Application::onDisconnectRequested);

    // Wire up receiver -> decoder -> preview
    connect(m_receiver.get(), &StreamReceiver::frameReceived,
            m_decoder.get(), &FrameDecoder::decode);
    connect(m_decoder.get(), &FrameDecoder::imageReady,
            m_window->previewWidget(), &CameraPreviewWidget::updateFrame);

#ifdef __linux__
    if (m_vcamWriter->open()) {
        connect(m_decoder.get(), &FrameDecoder::imageReady,
                m_vcamWriter.get(), &V4L2LoopbackWriter::writeFrame);
    } else {
        VC_WARN("Virtual camera not available, preview only");
    }
#endif

    // Connection state
    connect(m_receiver.get(), &StreamReceiver::connected, this, [this]() {
        VC_INFO("Stream connected");
        m_window->connectionPanel()->setConnected(true);
    });
    connect(m_receiver.get(), &StreamReceiver::disconnected, this, [this]() {
        VC_INFO("Stream disconnected");
        m_window->connectionPanel()->setConnected(false);
    });
    connect(m_receiver.get(), &StreamReceiver::errorOccurred, this, [](const QString &err) {
        VC_ERROR("Stream error: {}", err.toStdString());
    });

    // Restore last connection
    auto host = m_settings->lastHost();
    if (!host.isEmpty()) {
        VC_INFO("Restoring last device: {}:{}", host.toStdString(), m_settings->port());
        m_window->connectionPanel()->addDevice("Last", host, m_settings->port());
    }

    m_discovery->start();
    m_window->show();
    VC_INFO("Application initialized, window shown");
}

void Application::onDeviceFound(const QString &name, const QString &host, int port) {
    VC_INFO("Device discovered: {} at {}:{}", name.toStdString(), host.toStdString(), port);
    m_window->connectionPanel()->addDevice(name, host, port);
}

void Application::onConnectRequested(const QString &host, int port) {
    VC_INFO("Connecting to {}:{}", host.toStdString(), port);
    m_settings->setLastHost(host);
    m_settings->setPort(port);
    m_receiver->connectToHost(host, port);
}

void Application::onDisconnectRequested() {
    VC_INFO("Disconnect requested");
    m_receiver->disconnect();
}
