#include "viewmodels/AppController.h"
#include "ViewCamConfig.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "gpu/CudaBackend.h"
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
#include "MFVirtualCamManager.h"
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
      m_mfVirtualCam(std::make_unique<MFVirtualCamManager>()),
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

  // Detect CUDA runtime version once at startup (shown in About).
  {
    const std::string ver = CudaBackend::runtimeVersionString();
    if (!ver.empty())
        m_cudaVersion = QStringLiteral("CUDA ") + QString::fromStdString(ver);
  }

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
            // Include jpegQuality so the encoder preset takes effect immediately.
            static const int kQuality[] = {95, 85, 65}; // Quality/Balanced/Speed
            QJsonObject snap = m_cameraControl->snapshot();
            snap[QStringLiteral("jpegQuality")] =
                kQuality[qBound(0, m_settingsVm->encoderPreset(), 2)];
            m_receiver->sendControl(snap);
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
            if (!m_userDisconnect && !m_connection->sessionLimited())
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

  // Encoder preset -> phone JPEG quality (live update when connected)
  connect(m_settingsVm.get(), &SettingsViewModel::encoderPresetChanged, this,
          [this]() {
            if (!m_connection->isConnected()) return;
            static const int kQuality[] = {95, 85, 65};
            const int q = kQuality[qBound(0, m_settingsVm->encoderPreset(), 2)];
            m_receiver->sendControl(QJsonObject{{QStringLiteral("jpegQuality"), q}});
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
    // DirectShow filter — for OBS, VLC, legacy apps
    auto status = FilterRegistrar::checkStatus();
    VC_INFO("Filter status: {}",
            FilterRegistrar::statusText(status).toStdString());
    if (status == FilterRegistrar::Status::NotRegistered ||
        status == FilterRegistrar::Status::RegisteredStale) {
      VC_INFO("Registering DirectShow filter...");
      FilterRegistrar::registerFilter();
    }

    // Windows Virtual Camera API (Win 11 22H2+) — for Windows Camera, Teams, Zoom, Discord
    m_mfVirtualCam->registerAndStart(L"ViewCam Studio");
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

  // Cap at the max output resolution setting (0=720p, 1=1080p, 2=4K).
  static const int kResW[] = {1280, 1920, 3840};
  static const int kResH[] = {720,  1080, 2160};
  const int ri = qBound(0, m_settingsVm->maxResolution(), 2);
  QImage frame = (image.width() > kResW[ri] || image.height() > kResH[ri])
      ? image.scaled(kResW[ri], kResH[ri], Qt::KeepAspectRatio, Qt::SmoothTransformation)
      : image;

  const int bufSize = m_settingsVm->bufferedFrames();
  if (bufSize == 0) {
    // Flush any queued frames before passing the new one through.
    while (!m_frameBuffer.isEmpty())
      publishFrame(m_frameBuffer.takeFirst());
    publishFrame(frame);
  } else {
    // Jitter buffer: queue the incoming frame and display the oldest once
    // we have enough to absorb a burst. Caps at 2×bufSize to avoid unbounded
    // growth if the consumer stalls.
    m_frameBuffer.append(frame);
    while (m_frameBuffer.size() > bufSize * 2)
      m_frameBuffer.removeFirst();
    if (m_frameBuffer.size() > bufSize)
      publishFrame(m_frameBuffer.takeFirst());
  }
}

void AppController::publishFrame(const QImage &frame) {
  m_frameSource->publish(frame);
  if (m_virtualCam->available() && m_virtualCam->enabled())
    m_vcamWriter->writeFrame(frame);
}

void AppController::connectToDevice(const QString &name, const QString &host,
                                    int port) {
  VC_INFO("Connecting to {} at {}:{}", name.toStdString(), host.toStdString(),
          port);
  m_userDisconnect = false;
  m_sawFirstFrame = false;
  m_reconnectAttempts = 0;
  m_reconnectTimer.stop();
  m_frameBuffer.clear();
  m_settings->setLastHost(host);
  m_settings->setPort(port);
  m_connection->beginConnecting(name, host, port);
  m_receiver->connectToHost(host, port);
}

void AppController::connectManual(const QString &ip) {
  const QString host = ip.trimmed();
  if (host.isEmpty())
    return;
  connectToDevice(QStringLiteral("Manual (%1)").arg(host), host,
                  m_settingsVm->listenPort());
}

void AppController::disconnectDevice() {
  VC_INFO("Disconnect requested by user");
  m_userDisconnect = true;
  m_reconnectTimer.stop();
  m_frameBuffer.clear();
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
