#include "viewmodels/AppController.h"
#include "ViewCamConfig.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "network/DeviceDiscovery.h"
#include "network/FrameDecoder.h"
#include "network/StreamReceiver.h"
#include "viewmodels/ConnectionViewModel.h"
#include "viewmodels/DeviceListModel.h"
#include "viewmodels/FrameView.h"
#include "viewmodels/SettingsViewModel.h"
#include "viewmodels/VirtualCamViewModel.h"
#ifdef __linux__
#include "virtualcam/V4L2LoopbackWriter.h"
#elif defined(_WIN32)
#include "virtualcam/DirectShowVirtualCam.h"
#include "virtualcam/FilterRegistrar.h"
#endif

#include <QJsonObject>
#include <QQmlEngine>

AppController *AppController::s_instance = nullptr;

AppController::AppController(QObject *parent)
    : QObject(parent), m_settings(std::make_unique<Settings>()),
      m_receiver(std::make_unique<StreamReceiver>()),
      m_discovery(std::make_unique<DeviceDiscovery>()),
      m_decoder(std::make_unique<FrameDecoder>()),
#ifdef __linux__
      m_vcamWriter(std::make_unique<V4L2LoopbackWriter>()),
#elif defined(_WIN32)
      m_vcamWriter(std::make_unique<DirectShowVirtualCam>()),
#endif
      m_deviceModel(std::make_unique<DeviceListModel>()),
      m_connection(std::make_unique<ConnectionViewModel>()),
      m_virtualCam(std::make_unique<VirtualCamViewModel>()),
      m_settingsVm(std::make_unique<SettingsViewModel>(m_settings.get())),
      m_cameraControl(std::make_unique<CameraControlViewModel>()),
      m_frameSource(std::make_unique<FrameSource>()) {
  s_instance = this;
  VC_DEBUG("AppController created");
}

AppController::~AppController() {
  m_gpuBackend.reset(); // backends release their device/context in their dtor
  s_instance = nullptr;
  VC_DEBUG("AppController destroyed");
}

AppController *AppController::instance() { return s_instance; }

AppController *AppController::create(QQmlEngine *engine, QJSEngine *) {
  if (!s_instance) {
    VC_DEBUG("AppController singleton created by QML engine");
    s_instance = new AppController(engine);
    s_instance->init();
  }
  // Engine owns it for its lifetime; CppOwnership keeps the GC from touching
  // it.
  QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
  return s_instance;
}

void AppController::init() {
  VC_INFO("Initializing application controller");

  // GPU compute scaffold — pick a backend (CUDA/Vulkan/CPU) per hardware
  // policy + the GPU-processing setting, and re-pick when that toggle changes.
  selectGpuBackend();
  connect(m_settingsVm.get(), &SettingsViewModel::gpuProcessingChanged, this,
          [this]() {
            VC_INFO("GPU processing toggled {} — reselecting backend",
                    m_settingsVm->gpuProcessing() ? "ON" : "OFF");
            selectGpuBackend();
          });

  // discovery -> device model
  connect(m_discovery.get(), &DeviceDiscovery::deviceFound, m_deviceModel.get(),
          &DeviceListModel::addOrUpdate);

  // receiver -> stats + decoder
  connect(m_receiver.get(), &StreamReceiver::frameReceived, m_connection.get(),
          &ConnectionViewModel::onFrame);
  connect(m_receiver.get(), &StreamReceiver::frameReceived, m_decoder.get(),
          &FrameDecoder::decode);

  // decoder -> preview + virtual camera
  connect(m_decoder.get(), &FrameDecoder::imageReady, this,
          &AppController::onImageReady);

  // connection lifecycle — per protocol, "connected" is reached on HELLO, not
  // on the raw TCP socket coming up.
  connect(m_receiver.get(), &StreamReceiver::connected, this, [this]() {
    VC_INFO("TCP socket up, awaiting HELLO");
    m_reconnectAttempts = 0;
  });
  connect(m_receiver.get(), &StreamReceiver::helloReceived, this,
          [this](const QString &name, const QString &os, int, int, int battery,
                 bool charging, const QString &lens) {
            m_connection->setHelloInfo(name, os);
            m_connection->setPowerStatus(battery, charging);
            m_connection->setLens(lens);
            m_connection->markConnected();
            // Push the full control snapshot so the phone matches the panel.
            m_receiver->sendControl(m_cameraControl->snapshot());
          });
  // Periodic STATUS frames keep battery/charging current without reconnecting.
  connect(m_receiver.get(), &StreamReceiver::statusReceived, this,
          [this](int battery, bool charging) {
            m_connection->setPowerStatus(battery, charging);
          });
  // Lens descriptor: from HELLO and re-sent in STATUS on a lens flip.
  connect(m_receiver.get(), &StreamReceiver::lensReceived, this,
          [this](const QString &lens) { m_connection->setLens(lens); });
  // Phone-acknowledged control state (STATUS controls{}) -> reconcile the UI.
  connect(m_receiver.get(), &StreamReceiver::controlStateReceived,
          m_cameraControl.get(), &CameraControlViewModel::applyControls);
  // User toggles -> CONTROL frame patch to the phone.
  connect(m_cameraControl.get(), &CameraControlViewModel::controlPatch, this,
          [this](const QJsonObject &patch) { m_receiver->sendControl(patch); });
  connect(m_receiver.get(), &StreamReceiver::disconnected, this, [this]() {
    VC_INFO("Stream disconnected");
    const bool wasConnected = m_connection->isConnected();
    m_connection->markDisconnected();
    if (!m_userDisconnect && wasConnected && !m_connection->sessionLimited())
      scheduleReconnect();
  });
  connect(m_receiver.get(), &StreamReceiver::errorOccurred, this,
          [this](const QString &err) {
            VC_ERROR("Stream error: {}", err.toStdString());
            m_connection->markError(err);
            if (!m_userDisconnect)
              scheduleReconnect();
          });

  // settings -> discovery on/off
  connect(m_settingsVm.get(), &SettingsViewModel::autoDiscoveryChanged, this,
          [this]() {
            if (m_settingsVm->autoDiscovery())
              m_discovery->start();
            else
              m_discovery->stop();
          });

  m_reconnectTimer.setSingleShot(true);
  m_reconnectTimer.setInterval(RECONNECT_DELAY_MS);
  connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
    if (m_connection->state() != ConnectionViewModel::Disconnected)
      return;
    VC_INFO("Auto-reconnect attempt {} to {}:{}", m_reconnectAttempts,
            m_connection->host().toStdString(), m_connection->port());
    m_connection->beginConnecting(m_connection->deviceName(),
                                  m_connection->host(), m_connection->port());
    m_receiver->connectToHost(m_connection->host(), m_connection->port());
  });

  // virtual camera backend
#ifdef _WIN32
  {
    auto status = FilterRegistrar::checkStatus();
    VC_INFO("Filter status: {}",
            FilterRegistrar::statusText(status).toStdString());
    if (status == FilterRegistrar::Status::NotRegistered ||
        status == FilterRegistrar::Status::RegisteredStale) {
      VC_INFO("Attempting filter registration (UAC prompt)...");
      FilterRegistrar::registerFilter();
    }
  }
#endif
  if (m_vcamWriter->open()) {
#ifdef __linux__
    m_virtualCam->setAvailable(
        true, QString::fromStdString(m_vcamWriter->devicePath()));
#else
    m_virtualCam->setAvailable(true);
#endif
    VC_INFO("Virtual camera active");
  } else {
    m_virtualCam->setAvailable(false);
    VC_WARN("Virtual camera not available, preview only");
  }

  if (m_settingsVm->autoDiscovery())
    m_discovery->start();
}

void AppController::selectGpuBackend() {
  // Toggle OFF forces CPU; ON applies the auto policy (+ VIEWCAM_GPU_BACKEND).
  const GpuBackendKind kind =
      m_settingsVm->gpuProcessing() ? GpuBackendKind::Auto : GpuBackendKind::Cpu;
  m_gpuBackend = GpuBackendFactory::select(kind);

  const bool proof = m_gpuBackend->runProofOfLife();
  VC_INFO("GPU proof-of-life ({}): {}", m_gpuBackend->name(),
          proof ? "OK" : "FAILED");

  m_gpuBackendLabel = QString::fromLatin1(m_gpuBackend->name());
  const QString dev = QString::fromStdString(m_gpuBackend->device());
  if (!dev.isEmpty())
    m_gpuBackendLabel += QStringLiteral(" · ") + dev;
  emit gpuBackendChanged();
}

void AppController::onImageReady(const QImage &image) {
  if (!m_sawFirstFrame) {
    m_sawFirstFrame = true;
    VC_INFO("First video frame decoded: {}x{} -> preview + virtual camera",
            image.width(), image.height());
  }
  m_frameSource->publish(image);
  if (m_virtualCam->available() && m_virtualCam->enabled())
    m_vcamWriter->writeFrame(image);
}

void AppController::connectToDevice(const QString &name, const QString &host,
                                    int port) {
  VC_INFO("Connecting to {} at {}:{}", name.toStdString(), host.toStdString(),
          port);
  m_userDisconnect = false;
  m_sawFirstFrame = false;
  m_reconnectAttempts = 0;
  m_reconnectTimer.stop();
  m_settings->setLastHost(host);
  m_settings->setPort(port);
  m_connection->beginConnecting(name, host, port);
  m_receiver->connectToHost(host, port);
}

void AppController::connectManual(const QString &ip) {
  const QString host = ip.trimmed();
  if (host.isEmpty())
    return;
  connectToDevice(host, host, m_settingsVm->listenPort());
}

void AppController::disconnectDevice() {
  VC_INFO("Disconnect requested by user");
  m_userDisconnect = true;
  m_reconnectTimer.stop();
  m_receiver->disconnect();
  m_connection->markDisconnected();
  m_cameraControl->reset();
}

void AppController::scheduleReconnect() {
  if (!m_settingsVm->autoReconnect() || m_connection->host().isEmpty())
    return;
  if (++m_reconnectAttempts > RECONNECT_MAX_ATTEMPTS) {
    VC_WARN("Auto-reconnect gave up after {} attempts", RECONNECT_MAX_ATTEMPTS);
    m_reconnectAttempts = 0;
    return;
  }
  m_reconnectTimer.start();
}
