#include "viewmodels/AppController.h"
#include "ViewCamConfig.h"
#include "audio/AudioEncoder.h"
#include "audio/SystemAudioCapture.h"
#include "audio/VirtualMicSink.h"
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
#include "network/FirewallGuard.h"
#include <thread>
#endif

#include <cmath>

#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QQmlEngine>
#include <QStandardPaths>

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
      m_micSink(std::make_unique<VirtualMicSink>()),
      m_sysAudio(std::make_unique<SystemAudioCapture>(m_settings.get())),
      m_speakerEnc(std::make_unique<AudioEncoder>()),
      m_deviceModel(std::make_unique<DeviceListModel>()),
      m_connection(std::make_unique<ConnectionViewModel>()),
      m_virtualCam(std::make_unique<VirtualCamViewModel>()),
      m_settingsVm(std::make_unique<SettingsViewModel>(m_settings.get())),
      m_cameraControl(std::make_unique<CameraControlViewModel>()),
      m_audio(std::make_unique<AudioViewModel>(m_settings.get())),
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

  // discovery -> device model. While connected, ignore a beacon that advertises
  // the SAME phone (deviceId) at a DIFFERENT address: a multi-homed phone
  // (VPN / mobile data) beacons on its secondary interface too, which otherwise
  // shows up as a phantom duplicate device flickering next to the real one. The
  // live connection's own host is authoritative for that phone.
  connect(m_discovery.get(), &DeviceDiscovery::deviceFound, this,
          [this](const QString &deviceId, const QString &name, const QString &host,
                 int port) {
            if (m_connection->isConnected() && !m_connection->deviceId().isEmpty() &&
                deviceId == m_connection->deviceId() && host != m_connection->host())
              return;
            m_deviceModel->addOrUpdate(deviceId, name, host, port);
          });

  // receiver -> stats + decoder
  connect(m_receiver.get(), &StreamReceiver::frameReceived, m_connection.get(),
          &ConnectionViewModel::onFrame);
  connect(m_receiver.get(), &StreamReceiver::frameReceived, m_decoder.get(),
          &FrameDecoder::decode);

  // decoder -> preview + virtual camera
  connect(m_decoder.get(), &FrameDecoder::imageReady, this,
          &AppController::onImageReady);

  // H264 lost sync (mid-stream join / decode hiccup): ask the phone for an
  // IDR. Old phones ignore the unknown key — harmless.
  connect(m_decoder.get(), &FrameDecoder::keyframeNeeded, this, [this]() {
    m_receiver->sendControl(QJsonObject{{QStringLiteral("keyframe"), true}});
  });
  // Stateful H264 decoder must not carry reference frames across connections.
  connect(m_receiver.get(), &StreamReceiver::disconnected, m_decoder.get(),
          &FrameDecoder::resetStream);

  // connection lifecycle — per protocol, "connected" is reached on HELLO, not
  // on the raw TCP socket coming up.
  connect(m_receiver.get(), &StreamReceiver::connected, this, [this]() {
    VC_INFO("TCP socket up, awaiting HELLO");
    m_reconnectAttempts = 0;
    // Arm the watchdog for the TCP-up → HELLO gap: a phone that accepts the
    // socket but never sends HELLO would otherwise leave us in "Connecting"
    // forever (the watchdog used to start only on HELLO/frame/heartbeat).
    m_receiveWatchdog.start();
  });
  connect(m_receiver.get(), &StreamReceiver::helloReceived, this,
          [this](const QString &name, const QString &os, int, int, int battery,
                 bool charging, const QString &lens, bool) {
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
            // Codec negotiation: phones that see "h264" switch this
            // connection to hardware H264 (huge fps/battery/bitrate win);
            // older phones ignore the key and keep sending MJPEG.
            snap[QStringLiteral("codecs")] = advertisedCodecs();
            // Audio negotiation (spec §4.1): micEnabled starts the phone's
            // capture, speakerEnabled announces the return feed. Sent only to
            // audio-capable phones (audioCapableReceived fires before HELLO);
            // older phones would ignore the unknown keys anyway.
            if (m_audio->phoneAudioCapable()) {
              snap[QStringLiteral("micEnabled")] = m_audio->micEnabled();
              snap[QStringLiteral("speakerEnabled")] = m_audio->speakerEnabled();
            }
            m_receiver->sendControl(snap);
            updateSpeakerCapture();
          });
  // Phone Pro entitlement (HELLO + live STATUS): a paying user must not get the
  // free-tier watermark burned into their virtual camera, and 4K unlocks. On
  // disconnect we fall back to watermarked/gated.
  connect(m_receiver.get(), &StreamReceiver::proReceived, this,
          [this](bool pro) { m_connection->setPro(pro); });
  // Phone encoder capabilities (HELLO) → Settings protocol-picker gray-out.
  connect(m_receiver.get(), &StreamReceiver::phoneCodecsReceived, this,
          [this](const QStringList &codecs) { m_connection->setPhoneCodecs(codecs); });
  // All entitlement side effects hang off a SINGLE source of truth (proChanged),
  // so the disconnect AND error teardown paths — both funnel through
  // markDisconnected() -> setPro(false) — restore free-tier gating identically.
  // Previously the watermark reset lived only on the `disconnected` signal, so an
  // error-only teardown left the vcam watermark switched OFF for the next phone.
  connect(m_connection.get(), &ConnectionViewModel::proChanged, this, [this]() {
    const bool pro = m_connection->pro();
    m_vcamWriter->setWatermarkEnabled(!pro);
    if (m_settingsVm->gpuProcessing())
      selectGpuBackend(); // Pro-gated GPU: re-evaluate CPU/GPU on entitlement flip
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

  // ── Audio (spec §4.1) ────────────────────────────────────────────────────
  // Phone capability flag from HELLO — gates every audio path this session.
  connect(m_receiver.get(), &StreamReceiver::audioCapableReceived, this,
          [this](bool capable) {
            m_audio->setPhoneAudioCapable(capable);
            updateMicSink();
          });
  // Phone mic PCM -> input volume gain -> virtual mic device + level meter.
  connect(m_receiver.get(), &StreamReceiver::audioReceived, this,
          [this](const QByteArray &pcm, int, int) {
            if (!m_audio->micEnabled()) return; // mid-flight frames after mute
            const QByteArray out =
                applyGainPercent(pcm, m_settingsVm->micVolume());
            m_micSink->writeAudio(out);
            m_audio->reportMicChunk(out);
          });
  // Phone-acknowledged mic/speaker state (STATUS controls{}).
  connect(m_receiver.get(), &StreamReceiver::controlStateReceived,
          m_audio.get(), &AudioViewModel::applyControlEcho);
  // Phone's speaker codec preference (Audio tab Quality): switch the outgoing
  // feed between Opus (at the requested bitrate) and PCM live.
  connect(m_receiver.get(), &StreamReceiver::controlStateReceived, this,
          [this](const QJsonObject &controls) {
            if (controls.contains(QStringLiteral("speakerCodec")))
              m_speakerOpusWanted =
                  controls.value(QStringLiteral("speakerCodec")).toString() ==
                  QStringLiteral("opus");
            if (controls.contains(QStringLiteral("speakerBitrate"))) {
              const int b =
                  controls.value(QStringLiteral("speakerBitrate")).toInt(64000);
              m_speakerBitrate = qBound(6000, b, 512000);
            }
            m_opusFailedBitrate = 0; // fresh request — allow one open attempt
          });
  // Mic toggle (LiveView button) -> CONTROL patch; the phone starts/stops its
  // recorder, so a muted mic costs zero bandwidth AND zero phone battery.
  connect(m_audio.get(), &AudioViewModel::micEnabledChanged, this, [this]() {
    if (!m_audio->micEnabled()) m_audio->resetLevel();
    if (m_connection->isConnected() && m_audio->phoneAudioCapable())
      m_receiver->sendControl(
          QJsonObject{{QStringLiteral("micEnabled"), m_audio->micEnabled()}});
    updateMicSink();
  });
  // Speaker toggle -> announce, then start/stop the system-audio capture.
  connect(m_audio.get(), &AudioViewModel::speakerEnabledChanged, this, [this]() {
    if (m_connection->isConnected() && m_audio->phoneAudioCapable())
      m_receiver->sendControl(QJsonObject{
          {QStringLiteral("speakerEnabled"), m_audio->speakerEnabled()}});
    updateSpeakerCapture();
  });
  // Captured system audio -> speaker volume gain -> (Opus encode) -> phone.
  // Rate/channels come from the CAPTURE, not the wire default: Windows
  // WASAPI delivers the device mix rate (often 44100), and the phone honors
  // whatever the frame header says.
  connect(m_sysAudio.get(), &SystemAudioCapture::chunkReady, this,
          [this](const QByteArray &pcm) {
            const int rate = m_sysAudio->sampleRate();
            const int channels = m_sysAudio->channels();
            const QByteArray out =
                applyGainPercent(pcm, m_settingsVm->speakerVolume());
            // Codec follows the phone's live preference (Audio tab Quality):
            // Opus at the chosen bitrate, PCM otherwise or when this build
            // has no encoder. A failed open latches to PCM until the request
            // changes — retrying every 20 ms chunk would just spam.
            if (m_speakerOpusWanted && m_speakerBitrate != m_opusFailedBitrate) {
              if (!m_speakerEnc->isOpen() ||
                  m_speakerEnc->bitrate() != m_speakerBitrate) {
                // libopus only accepts 8/12/16/24/48 kHz input; a 44.1 kHz
                // mix falls back to PCM rather than resampling here.
                if (!m_speakerEnc->open(rate, channels, m_speakerBitrate))
                  m_opusFailedBitrate = m_speakerBitrate;
              }
              if (m_speakerEnc->isOpen()) {
                const auto packets = m_speakerEnc->encode(out);
                for (const QByteArray &p : packets)
                  m_receiver->sendAudio(p, rate, channels,
                                        vc::FrameFormat::AudioOpus);
                return;
              }
            } else if (!m_speakerOpusWanted && m_speakerEnc->isOpen()) {
              m_speakerEnc->close();
            }
            m_receiver->sendAudio(out, rate, channels);
          });
  // The Sources toggle persists via Settings; mirror it into runtime state.
  m_audio->setSpeakerEnabled(m_settingsVm->captureSystemAudio());
  connect(m_settingsVm.get(), &SettingsViewModel::captureSystemAudioChanged, this,
          [this]() { m_audio->setSpeakerEnabled(m_settingsVm->captureSystemAudio()); });
  // "Also play on this computer" flip mid-stream: restart the capture so it
  // re-routes (shared @DEFAULT_MONITOR@ ↔ exclusive null-sink) atomically.
  connect(m_settingsVm.get(), &SettingsViewModel::localPlaybackChanged, this,
          [this]() {
            if (m_sysAudio->isRunning()) m_sysAudio->stop();
            updateSpeakerCapture();
          });
  // User toggles -> CONTROL frame patch to the phone.
  connect(m_cameraControl.get(), &CameraControlViewModel::controlPatch, this,
          [this](const QJsonObject &patch) { m_receiver->sendControl(patch); });
  connect(m_receiver.get(), &StreamReceiver::disconnected, this, [this]() {
    VC_INFO("Stream disconnected");
    // Blank the preview NOW: the last decoded frame otherwise lingers in
    // FrameView and flashes into the next connection (potentially another
    // phone's image) until that session's first frame decodes. Also drop any
    // queued frames so they can't replay into the new session.
    m_frameBuffer.clear();
    m_frameSource->publish(QImage());
    // Audio teardown mirrors the video path: stop feeding the phone, drop the
    // per-session capability, zero the meter, and unload the virtual mic (a
    // writer-less pipe source must never linger — it wedges PipeWire).
    m_audio->setPhoneAudioCapable(false);
    m_audio->applyControlEcho(QJsonObject{{QStringLiteral("mic"), false},
                                          {QStringLiteral("speaker"), false},
                                          {QStringLiteral("micPermission"), true}});
    updateSpeakerCapture();
    updateMicSink();
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

  // Stream protocol setting -> re-advertise codecs live, so flipping the
  // Settings combo switches the phone's encoder mid-connection (no reconnect).
  // The panel's Connection row reflects what actually arrives next.
  connect(m_settingsVm.get(), &SettingsViewModel::streamProtocolChanged, this,
          [this]() {
            if (!m_connection->isConnected()) return;
            m_receiver->sendControl(
                QJsonObject{{QStringLiteral("codecs"), advertisedCodecs()}});
          });

  // Dead-connection watchdog: if no data arrives for 5 s after HELLO, abort
  // the socket so the reconnect cycle fires (covers silent TCP loss / NAT expiry).
  m_receiveWatchdog.setSingleShot(true);
  m_receiveWatchdog.setInterval(RECEIVE_TIMEOUT_MS);
  connect(&m_receiveWatchdog, &QTimer::timeout, this, [this]() {
    VC_WARN("No data from phone for {}ms — forcing disconnect", RECEIVE_TIMEOUT_MS);
    m_receiver->disconnect();
  });
  connect(m_receiver.get(), &StreamReceiver::helloReceived,
          this, [this](auto...) { m_receiveWatchdog.start(); });
  connect(m_receiver.get(), &StreamReceiver::frameReceived,
          this, [this](const FrameData &) { m_receiveWatchdog.start(); });
  connect(m_receiver.get(), &StreamReceiver::heartbeatReceived,
          this, [this]() { m_receiveWatchdog.start(); });
  connect(m_receiver.get(), &StreamReceiver::audioReceived,
          this, [this](const QByteArray &, int, int) { m_receiveWatchdog.start(); });
  connect(m_receiver.get(), &StreamReceiver::statusReceived,
          this, [this](int, bool) { m_receiveWatchdog.start(); });
  connect(m_receiver.get(), &StreamReceiver::disconnected,
          this, [this]() { m_receiveWatchdog.stop(); });

  m_reconnectTimer.setSingleShot(true);
  m_reconnectTimer.setInterval(RECONNECT_DELAY_MS);
  connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
    if (m_connection->state() != ConnectionViewModel::Disconnected)
      return;
    // Chase the phone's CURRENT address: if it changed IP (DHCP renew / Wi-Fi
    // roam) it's re-advertising the new host under the same deviceId. Reusing
    // the frozen original host would just retry a dead IP until we give up,
    // while the phone sits right there discoverable. Manual/empty-deviceId
    // connects have no beacon, so they keep the stored host.
    QString host = m_connection->host();
    int port = m_connection->port();
    QString freshHost;
    int freshPort = 0;
    if (m_deviceModel->lookupHost(m_connection->deviceId(), freshHost, freshPort)) {
      host = freshHost;
      port = freshPort;
    }
    VC_INFO("Auto-reconnect attempt {} to {}:{}", m_reconnectAttempts,
            host.toStdString(), port);
    m_connection->beginConnecting(m_connection->deviceName(), host, port,
                                  m_connection->deviceId());
    m_receiver->connectToHost(host, port);
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

  // Inbound UDP discovery beacon needs a firewall exception. The installer
  // creates one at install time (elevated), but a dev build, a firewall
  // reset, or a manually deleted rule leaves phones silently never appearing
  // with no obvious symptom — surface a fix affordance instead (fixFirewall()).
  m_firewallBlocked = FirewallGuard::checkDiscoveryStatus() != FirewallGuard::Status::Allowed;
  if (m_firewallBlocked)
    VC_WARN("Discovery may be blocked by Windows Firewall — no approved inbound rule found");
#endif
  // Deferred: open() can block for many seconds on first run (Linux: pkexec
  // prompt + modprobe wait inside ensureModuleLoaded). init() runs before the
  // QML window loads, so opening synchronously here froze startup and could
  // pop a privilege dialog with no app window behind it. singleShot(0) queues
  // it behind the initial QML load, after the window is up.
  QTimer::singleShot(0, this, [this]() {
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
  });
  // NB: the virtual microphone is NOT opened here. A pipe source that sits
  // loaded with no writer destabilizes PipeWire's graph clock and can break
  // ALL system audio, so the device lives only while phone mic audio
  // actually flows — see updateMicSink().
  //
  // Crash aftermath repair (deferred, same reasoning as the vcam open): if a
  // previous run died mid-session it may have left a viewcam module loaded —
  // worst case the user's default sink is still our null sink, i.e. total
  // silence. These touch the sound server ONLY when such leftovers exist.
  QTimer::singleShot(0, this, [this]() {
    SystemAudioCapture::recoverStaleRouting(m_settings.get());
    m_micSink->cleanupStale();
  });
  // Dev-only harness (VIEWCAM_MIC_TEST=1): open the virtual mic with no phone
  // and feed it a 440 Hz tone, so the node + latency path can be verified
  // with parec alone. Never set in production launches.
  if (qEnvironmentVariableIsSet("VIEWCAM_MIC_TEST")) {
    QTimer::singleShot(0, this, [this]() {
      m_audio->setVirtualMicReady(
          m_micSink->open(vc::kAudioSampleRate, vc::kMicChannels));
      auto *tone = new QTimer(this);
      tone->setTimerType(Qt::PreciseTimer); // coarse timers fake wire jitter
      tone->setInterval(20);
      connect(tone, &QTimer::timeout, this, [this]() {
        static int phase = 0;
        QByteArray pcm(960 * 2, 0);
        auto *s = reinterpret_cast<qint16 *>(pcm.data());
        for (int i = 0; i < 960; ++i)
          s[i] = qint16(12000 *
                        std::sin(6.283185307179586 * 440.0 * (phase + i) / 48000.0));
        phase += 960;
        m_micSink->writeAudio(pcm);
      });
      tone->start();
    });
  }

  if (m_settingsVm->autoDiscovery())
    m_discovery->start();
}

void AppController::selectGpuBackend() {
  // Toggle OFF forces CPU; ON applies the auto policy (+ VIEWCAM_GPU_BACKEND).
  // GPU processing is Pro-gated: a free session forces CPU regardless of the
  // persisted toggle, same anti-leak reasoning as the 4K cap in onImageReady().
  const bool gpuOn = m_settingsVm->gpuProcessing() && m_connection->pro();
  const GpuBackendKind kind =
      gpuOn ? GpuBackendKind::Auto : GpuBackendKind::Cpu;
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

/**
 * Decoder codecs offered to the phone, honoring the Settings "Stream
 * protocol" choice: MJPEG (index 0) forces the universal codec; H.264/H.265
 * (1/2) offer h264 when this build can decode it (H.265 has no pipeline yet,
 * so it behaves as H.264 rather than silently breaking the stream).
 */
QJsonArray AppController::advertisedCodecs() const {
  QJsonArray codecs{QStringLiteral("mjpeg")};
  if (FrameDecoder::h264Supported() && m_settingsVm->streamProtocol() >= 1)
    codecs.append(QStringLiteral("h264"));
  return codecs;
}

void AppController::onImageReady(const QImage &image) {
  if (!m_sawFirstFrame) {
    m_sawFirstFrame = true;
    VC_INFO("First video frame decoded: {}x{} -> preview + virtual camera",
            image.width(), image.height());
  }

  // Aspect-ratio preset: centered crop AFTER decode. The phone only pre-crops
  // the MJPEG path (where each frame is independent); H264 always arrives as
  // the full frame because resizing the phone's hardware encoder mid-stream
  // desynced the decoder. Cropping here is a cheap lossless copy, makes both
  // codecs behave identically, and works even for phones that don't understand
  // the "aspect" control at all. No-op when the frame already matches (the
  // MJPEG pre-crop case) or the ratio is "full". Frames are already upright
  // here (FrameDecoder rotates), so the ratio applies directly.
  QImage src = image;
  const QString ar = m_cameraControl->aspectRatio();
  if (ar != QStringLiteral("full") && !src.isNull()) {
    const QStringList parts = ar.split(QLatin1Char(':'));
    if (parts.size() == 2) {
      const double target = parts[0].toDouble() / qMax(1.0, parts[1].toDouble());
      const double have = double(src.width()) / double(src.height());
      if (target > 0.0 && std::abs(have - target) > 0.01) {
        int cw = src.width();
        int ch = src.height();
        if (have > target)
          cw = int(src.height() * target) & ~1;
        else
          ch = int(src.width() / target) & ~1;
        cw = qBound(2, cw, src.width());
        ch = qBound(2, ch, src.height());
        src = src.copy((src.width() - cw) / 2, (src.height() - ch) / 2, cw, ch);
      }
    }
  }

  // Cap at the max output resolution setting (0=720p, 1=1080p, 2=4K).
  static const int kResW[] = {1280, 1920, 3840};
  static const int kResH[] = {720,  1080, 2160};
  int ri = qBound(0, m_settingsVm->maxResolution(), 2);
  // 4K is a Pro feature. A free session (no phone Pro entitlement) is capped at
  // 1080p regardless of the persisted setting — otherwise a device that once had
  // a Pro phone connected keeps its 4K ceiling forever. Read live per frame so
  // the cap re-applies the instant Pro ends (the QML row gate is cosmetic only).
  if (!m_connection->pro())
    ri = qMin(ri, 1);
  QImage frame = (src.width() > kResW[ri] || src.height() > kResH[ri])
      ? src.scaled(kResW[ri], kResH[ri], Qt::KeepAspectRatio, Qt::SmoothTransformation)
      : src;

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
  // Apply the user "Mirror image" flip to the FRAME (not just the preview's
  // sourceRect) so the preview AND the virtual-camera output agree — otherwise
  // apps like Zoom/OBS saw the un-mirrored feed while the local preview was
  // flipped. Front-camera mirroring is already baked in by FrameDecoder.
  QImage out = frame;
  if (m_settingsVm->mirrorImage()) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    out = frame.flipped(Qt::Horizontal);
#else
    out = frame.mirrored(true, false);
#endif
  }
  m_lastFrame = out; // keep the composited frame for snapshots
  m_frameSource->publish(out);
  if (m_virtualCam->available() && m_virtualCam->enabled())
    m_vcamWriter->writeFrame(out);
}

QString AppController::saveSnapshot() {
  if (m_lastFrame.isNull()) {
    VC_WARN("Snapshot requested but no live frame");
    emit snapshotSaved(QString());
    return QString();
  }
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
      QStringLiteral("/ViewCam");
  QDir().mkpath(dir);
  const QString path =
      dir + QStringLiteral("/ViewCam-") +
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) +
      QStringLiteral(".png");
  if (!m_lastFrame.save(path, "PNG")) {
    VC_ERROR("Snapshot save failed: {}", path.toStdString());
    emit snapshotSaved(QString());
    return QString();
  }
  VC_INFO("Snapshot saved: {}", path.toStdString());
  emit snapshotSaved(path);
  return path;
}

void AppController::connectToDevice(const QString &name, const QString &host,
                                    int port, const QString &deviceId) {
  VC_INFO("Connecting to {} at {}:{}", name.toStdString(), host.toStdString(),
          port);
  m_userDisconnect = false;
  m_sawFirstFrame = false;
  m_reconnectAttempts = 0;
  m_reconnectTimer.stop();
  m_frameBuffer.clear();
  m_settings->setLastHost(host);
  m_settings->setPort(port);
  m_connection->beginConnecting(name, host, port, deviceId);
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

void AppController::fixFirewall() {
#ifdef _WIN32
  // requestApproval() blocks on the UAC prompt + netsh — run it off the UI
  // thread so the window stays responsive while the user answers the prompt.
  std::thread([this]() {
    FirewallGuard::requestApproval();
    const bool stillBlocked =
        FirewallGuard::checkDiscoveryStatus() != FirewallGuard::Status::Allowed;
    QMetaObject::invokeMethod(
        this,
        [this, stillBlocked]() {
          if (m_firewallBlocked != stillBlocked) {
            m_firewallBlocked = stillBlocked;
            emit firewallBlockedChanged();
          }
        },
        Qt::QueuedConnection);
  }).detach();
#endif
}

QByteArray AppController::applyGainPercent(const QByteArray &pcm, int percent) {
  const int p = qBound(0, percent, 200);
  if (p == 100 || pcm.isEmpty()) return pcm;
  QByteArray out(pcm);
  auto *s = reinterpret_cast<qint16 *>(out.data());
  const int n = int(out.size() / 2);
  for (int i = 0; i < n; ++i) {
    const int v = int(s[i]) * p / 100;
    s[i] = qint16(qBound(-32768, v, 32767));
  }
  return out;
}

void AppController::updateMicSink() {
  // The "ViewCam Microphone" pipe source exists ONLY while phone mic audio is
  // actually being delivered. Loaded idle (no FIFO writer) it destabilizes
  // PipeWire's graph clock and breaks unrelated system audio — so its
  // lifetime is connect(+mic on) → disconnect, never app launch → quit.
  const bool shouldRun = m_connection->isConnected() &&
                         m_audio->phoneAudioCapable() &&
                         m_audio->micEnabled();
  if (shouldRun && !m_micSink->isOpen())
    m_audio->setVirtualMicReady(
        m_micSink->open(vc::kAudioSampleRate, vc::kMicChannels));
  else if (!shouldRun && m_micSink->isOpen()) {
    m_micSink->close();
    m_audio->setVirtualMicReady(false);
  }
}

void AppController::updateSpeakerCapture() {
  const bool shouldRun = m_connection->isConnected() &&
                         m_audio->phoneAudioCapable() &&
                         m_audio->speakerEnabled();
  if (shouldRun && !m_sysAudio->isRunning()) {
    m_sysAudio->start(vc::kAudioSampleRate, vc::kSpeakerChannels,
                      /*exclusive=*/!m_settingsVm->localPlayback());
  } else if (!shouldRun && m_sysAudio->isRunning()) {
    m_sysAudio->stop();
    m_speakerEnc->close();
  }
  // Truthful UI: "enabled but not capturing" means this computer has no
  // output device to tap (zero-speaker PC) — the row says so instead of
  // pretending the phone is playing.
  m_audio->setSpeakerCaptureRunning(m_sysAudio->isRunning());
}

void AppController::scheduleReconnect() {
  if (!m_settingsVm->autoReconnect() || m_connection->host().isEmpty())
    return;
  // A single failed connection commonly fires BOTH disconnected and errorOccurred;
  // if a reconnect is already pending, don't count the same failure twice (which
  // would make us give up at half the intended attempts).
  if (m_reconnectTimer.isActive())
    return;
  if (++m_reconnectAttempts > RECONNECT_MAX_ATTEMPTS) {
    VC_WARN("Auto-reconnect gave up after {} attempts", RECONNECT_MAX_ATTEMPTS);
    m_reconnectAttempts = 0;
    return;
  }
  m_reconnectTimer.start();
}
