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
#elif defined(_WIN32)
#include "virtualcam/DirectShowVirtualCam.h"
#include "virtualcam/FilterRegistrar.h"
#endif
#include "ui/SettingsTab.h"

Application::Application(QObject *parent)
    : QObject(parent)
    , m_settings(std::make_unique<Settings>(this))
    , m_receiver(std::make_unique<StreamReceiver>(this))
    , m_discovery(std::make_unique<DeviceDiscovery>(this))
    , m_decoder(std::make_unique<FrameDecoder>(this))
    , m_window(std::make_unique<MainWindow>(m_settings.get()))
#ifdef __linux__
    , m_vcamWriter(std::make_unique<V4L2LoopbackWriter>(this))
#elif defined(_WIN32)
    , m_vcamWriter(std::make_unique<DirectShowVirtualCam>(this))
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

    // Track device name when a card is clicked so we can show it on the camera screen
    connect(m_window->connectionPanel(), &ConnectionPanel::deviceCardClicked,
            this, [this](const QString &name, const QString &, int) {
        m_connectedDeviceName = name;
    });

    // Wire up receiver -> decoder -> preview
    connect(m_receiver.get(), &StreamReceiver::frameReceived,
            m_decoder.get(), &FrameDecoder::decode);
    connect(m_decoder.get(), &FrameDecoder::imageReady,
            m_window->previewWidget(), &CameraPreviewWidget::updateFrame);

    // ── Virtual camera setup ─────────────────────────────────────────────────
#ifdef __linux__
    if (m_vcamWriter->open()) {
        connect(m_decoder.get(), &FrameDecoder::imageReady,
                m_vcamWriter.get(), &V4L2LoopbackWriter::writeFrame);
        m_window->setVirtualCamStatus(true);
        VC_INFO("V4L2 virtual camera active");
    } else {
        m_window->setVirtualCamStatus(false, true);
        VC_WARN("Virtual camera not available, preview only");
    }
#elif defined(_WIN32)
    // Auto-register DirectShow filter if needed
    {
        auto status = FilterRegistrar::checkStatus();
        VC_INFO("Filter status: {}", FilterRegistrar::statusText(status).toStdString());

        if (status == FilterRegistrar::Status::NotRegistered ||
            status == FilterRegistrar::Status::RegisteredStale) {
            VC_INFO("Attempting filter registration (UAC prompt)...");
            if (FilterRegistrar::registerFilter()) {
                status = FilterRegistrar::checkStatus();
                VC_INFO("Filter registration result: {}",
                        FilterRegistrar::statusText(status).toStdString());
            }
        }

        // Pass registrar info to settings tab
        m_window->settingsTab()->updateFilterStatus();
    }

    if (m_vcamWriter->open()) {
        connect(m_decoder.get(), &FrameDecoder::imageReady,
                m_vcamWriter.get(), &DirectShowVirtualCam::writeFrame);
        m_window->setVirtualCamStatus(true);
        VC_INFO("DirectShow virtual camera active (shared memory)");
    } else {
        m_window->setVirtualCamStatus(false, true);
        VC_WARN("Virtual camera not available, preview only");
    }
#endif

    // Session limit (free tier) -> status update
    connect(m_receiver.get(), &StreamReceiver::sessionLimitReached, this, [this]() {
        VC_INFO("Session limit reached (Free tier)");
        m_window->connectionPanel()->showSessionLimitMessage();
        m_window->setStatusText(tr("Session ended (Free tier limit)"));
    });

    // Connection state -> screen navigation + status
    connect(m_receiver.get(), &StreamReceiver::connected, this, [this]() {
        VC_INFO("Stream connected");
        m_window->connectionPanel()->setConnected(true);
        m_window->setStatusText(tr("Connected"));
        // Navigate to the camera screen
        m_window->showCameraScreen(m_connectedDeviceName);
    });
    connect(m_receiver.get(), &StreamReceiver::disconnected, this, [this]() {
        VC_INFO("Stream disconnected");
        m_window->connectionPanel()->setConnected(false);
        m_window->setStatusText(tr("Disconnected"));
        m_window->setFpsText("");
        // Navigate back to the devices screen
        m_window->showDevicesScreen();
    });
    connect(m_receiver.get(), &StreamReceiver::errorOccurred, this, [this](const QString &err) {
        VC_ERROR("Stream error: {}", err.toStdString());
        m_window->setStatusText(tr("Error: ") + err);
        // Return to device list on error
        m_window->showDevicesScreen();
    });

    // FPS updates -> camera screen chip
    connect(m_receiver.get(), &StreamReceiver::frameReceived,
            this, [this](const auto &) {
        // FPS is computed in StreamReceiver — we rely on setFpsText calls from
        // the receiver's FPS signal. If the receiver exposes an fpsUpdated signal,
        // connect it here. For now we leave a hook via setFpsText.
    });

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
    // If no device name was set (manual connect), use the host as the name
    if (m_connectedDeviceName.isEmpty())
        m_connectedDeviceName = host;
    m_receiver->connectToHost(host, port);
}

void Application::onDisconnectRequested() {
    VC_INFO("Disconnect requested");
    m_connectedDeviceName.clear();
    m_receiver->disconnect();
}
