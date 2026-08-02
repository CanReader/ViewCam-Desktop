#pragma once

#include <QImage>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
#include <memory>

// Full includes (not forward declarations): the Q_PROPERTY pointer types
// must be complete where the generated moc/registration code is compiled.
#include "gpu/GpuBackend.h"
#include "viewmodels/AudioViewModel.h"
#include "viewmodels/CameraControlViewModel.h"
#include "viewmodels/ConnectionViewModel.h"
#include "viewmodels/DeviceListModel.h"
#include "viewmodels/FrameView.h"
#include "viewmodels/SettingsViewModel.h"
#include "viewmodels/VirtualCamViewModel.h"

class QQmlEngine;
class QJSEngine;

class Settings;
class StreamReceiver;
class DeviceDiscovery;
class FrameDecoder;
class VirtualMicSink;
class SystemAudioCapture;
class AudioEncoder;
#ifdef __linux__
class V4L2LoopbackWriter;
#elif defined(_WIN32)
class DirectShowVirtualCam;
class MFVirtualCamManager;
#endif

// Root facade for QML. Owns the network/virtualcam backend and the
// ViewModels; the UI binds to ViewModels and calls the Q_INVOKABLEs here —
// it never touches sockets or the v4l2 writer directly.
class AppController : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(DeviceListModel *devices READ devices CONSTANT)
    Q_PROPERTY(ConnectionViewModel *connection READ connection CONSTANT)
    Q_PROPERTY(VirtualCamViewModel *virtualCam READ virtualCam CONSTANT)
    Q_PROPERTY(SettingsViewModel *settings READ settings CONSTANT)
    Q_PROPERTY(CameraControlViewModel *cameraControl READ cameraControl CONSTANT)
    Q_PROPERTY(AudioViewModel *audio READ audio CONSTANT)
    Q_PROPERTY(FrameSource *frameSource READ frameSource CONSTANT)
    // Active GPU compute backend label (e.g. "CUDA · NVIDIA RTX 4050").
    Q_PROPERTY(QString gpuBackend READ gpuBackend NOTIFY gpuBackendChanged)
    // CUDA runtime version string, e.g. "CUDA 12.4". Empty when CUDA unavailable.
    Q_PROPERTY(QString cudaVersion READ cudaVersion CONSTANT)
    // Currently active navigation page ("liveview" / "sources" / "settings").
    // Persists across QML hot-reloads so the user stays on the same screen.
    Q_PROPERTY(QString activePage READ activePage WRITE setActivePage NOTIFY activePageChanged)
    // Windows only: true when Windows Firewall has no approved inbound rule
    // for the discovery beacon, so phones silently never appear. Always
    // false on other platforms.
    Q_PROPERTY(bool firewallBlocked READ firewallBlocked NOTIFY firewallBlockedChanged)

public:
    ~AppController() override;

    // QML_SINGLETON factory. The constructor is private so the type is NOT
    // default-constructible — this forces Qt's singletonConstructionMode() to
    // pick FactoryWrapper/Factory mode and actually call create(), instead of
    // silently default-constructing its own (un-init'd) instance.
    static AppController *create(QQmlEngine *, QJSEngine *);
    static AppController *instance();

    DeviceListModel *devices() const { return m_deviceModel.get(); }
    ConnectionViewModel *connection() const { return m_connection.get(); }
    VirtualCamViewModel *virtualCam() const { return m_virtualCam.get(); }
    SettingsViewModel *settings() const { return m_settingsVm.get(); }
    CameraControlViewModel *cameraControl() const { return m_cameraControl.get(); }
    AudioViewModel *audio() const { return m_audio.get(); }
    FrameSource *frameSource() const { return m_frameSource.get(); }
    QString gpuBackend() const { return m_gpuBackendLabel; }
    QString cudaVersion() const { return m_cudaVersion; }

    QString activePage() const { return m_activePage; }
    void setActivePage(const QString &page) {
        if (m_activePage == page) return;
        m_activePage = page;
        emit activePageChanged();
    }

    bool firewallBlocked() const { return m_firewallBlocked; }

    Q_INVOKABLE void connectToDevice(const QString &name, const QString &host, int port,
                                     const QString &deviceId = QString());
    Q_INVOKABLE void connectManual(const QString &ip);
    Q_INVOKABLE void disconnectDevice();
    // Re-checked (Windows only) after the user asks to fix it — see
    // fixFirewall(). No-op on other platforms.
    Q_INVOKABLE void fixFirewall();
    // Save the current live frame (post-mirror, exactly what apps receive) to
    // the user's Pictures/ViewCam folder as a timestamped PNG. Returns the saved
    // path, or an empty string if there's no frame / the write failed.
    Q_INVOKABLE QString saveSnapshot();

signals:
    void gpuBackendChanged();
    void activePageChanged();
    void firewallBlockedChanged();
    // path is empty on failure (no live frame or write error).
    void snapshotSaved(const QString &path);

private:
    explicit AppController(QObject *parent = nullptr);
    void init();
    QJsonArray advertisedCodecs() const;
    void onImageReady(const QImage &image);
    void publishFrame(const QImage &frame);
    void scheduleReconnect();
    // Start/stop the system-audio → phone speaker feed to match the current
    // (connected && phone-capable && enabled) state.
    void updateSpeakerCapture();
    // Load/unload the "ViewCam Microphone" pipe source to match that same
    // state. Never left loaded while idle — a writer-less pipe source
    // destabilizes PipeWire and can break ALL system audio.
    void updateMicSink();
    // Software volume for the audio streams: percent 0-200, 100 = unity,
    // s16 samples scaled with hard clip. Returns the input unchanged at 100.
    static QByteArray applyGainPercent(const QByteArray &pcm, int percent);

    static AppController *s_instance;

    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<StreamReceiver> m_receiver;
    std::unique_ptr<DeviceDiscovery> m_discovery;
    std::unique_ptr<FrameDecoder> m_decoder;
#ifdef __linux__
    std::unique_ptr<V4L2LoopbackWriter>   m_vcamWriter;
#elif defined(_WIN32)
    std::unique_ptr<DirectShowVirtualCam> m_vcamWriter;
    std::unique_ptr<MFVirtualCamManager>  m_mfVirtualCam;
#endif

    std::unique_ptr<VirtualMicSink> m_micSink;
    std::unique_ptr<SystemAudioCapture> m_sysAudio;
    std::unique_ptr<AudioEncoder> m_speakerEnc;
    // Phone's speaker codec preference from STATUS controls{} (spec §4.1).
    bool m_speakerOpusWanted = false;
    int m_speakerBitrate = 64000;
    // Bitrate whose encoder open failed — latches PCM fallback until the
    // phone asks for something new (no per-chunk retry spam).
    int m_opusFailedBitrate = 0;

    std::unique_ptr<DeviceListModel> m_deviceModel;
    std::unique_ptr<ConnectionViewModel> m_connection;
    std::unique_ptr<VirtualCamViewModel> m_virtualCam;
    std::unique_ptr<SettingsViewModel> m_settingsVm;
    std::unique_ptr<CameraControlViewModel> m_cameraControl;
    std::unique_ptr<AudioViewModel> m_audio;
    std::unique_ptr<FrameSource> m_frameSource;

    // Last frame published to preview/vcam (post-mirror) — the snapshot source.
    QImage m_lastFrame;

    // (Re)pick the GPU compute backend per the hardware policy + the
    // GPU-processing setting, run its proof-of-life, and log the choice.
    void selectGpuBackend();

    // Active GPU compute backend (CUDA / Vulkan compute / CPU). See
    // GPU_ABSTRACTION_BRIEF — the app only ever talks to this interface.
    std::unique_ptr<GpuBackend> m_gpuBackend;
    QString m_gpuBackendLabel;
    QString m_cudaVersion;
    QString m_activePage = QStringLiteral("liveview");
    bool m_firewallBlocked = false;

    // Jitter buffer: holds up to bufferedFrames decoded frames for smooth display.
    QList<QImage> m_frameBuffer;

    QTimer m_reconnectTimer;
    QTimer m_receiveWatchdog;   // fires if no data arrives for RECEIVE_TIMEOUT_MS
    bool m_userDisconnect = false;
    bool m_sawFirstFrame = false;
    int m_reconnectAttempts = 0;

    static constexpr int RECONNECT_DELAY_MS    = 2500;
    static constexpr int RECONNECT_MAX_ATTEMPTS = 5;
    static constexpr int RECEIVE_TIMEOUT_MS    = 5000;
};
