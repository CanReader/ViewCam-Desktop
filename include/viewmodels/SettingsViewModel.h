#pragma once

#include <QColor>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class Settings;

// Every user preference from the Settings / Sources / Optimization surfaces,
// persisted through core/Settings (QSettings). moc cannot expand macros for
// Q_PROPERTY, so the boilerplate is spelled out.
class SettingsViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")

    // Appearance
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)
    Q_PROPERTY(QString accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool telemetryOverlay READ telemetryOverlay WRITE setTelemetryOverlay NOTIFY telemetryOverlayChanged)
    Q_PROPERTY(bool compactSidebar READ compactSidebar WRITE setCompactSidebar NOTIFY compactSidebarChanged)
    Q_PROPERTY(bool launchAtLogin READ launchAtLogin WRITE setLaunchAtLogin NOTIFY launchAtLoginChanged)

    // Stream engine
    Q_PROPERTY(bool hardwareAccel READ hardwareAccel WRITE setHardwareAccel NOTIFY hardwareAccelChanged)
    // GPU processing = the frame filter/compute path (CUDA/Vulkan); distinct
    // from hardwareAccel which is GPU *video encoding*.
    Q_PROPERTY(bool gpuProcessing READ gpuProcessing WRITE setGpuProcessing NOTIFY gpuProcessingChanged)
    Q_PROPERTY(int streamProtocol READ streamProtocol WRITE setStreamProtocol NOTIFY streamProtocolChanged)
    Q_PROPERTY(int encoderPreset READ encoderPreset WRITE setEncoderPreset NOTIFY encoderPresetChanged)
    Q_PROPERTY(int maxResolution READ maxResolution WRITE setMaxResolution NOTIFY maxResolutionChanged)
    Q_PROPERTY(int keyframeInterval READ keyframeInterval WRITE setKeyframeInterval NOTIFY keyframeIntervalChanged)
    Q_PROPERTY(int bufferedFrames READ bufferedFrames WRITE setBufferedFrames NOTIFY bufferedFramesChanged)

    // Network
    Q_PROPERTY(bool autoDiscovery READ autoDiscovery WRITE setAutoDiscovery NOTIFY autoDiscoveryChanged)
    Q_PROPERTY(int listenPort READ listenPort WRITE setListenPort NOTIFY listenPortChanged)
    Q_PROPERTY(bool autoReconnect READ autoReconnect WRITE setAutoReconnect NOTIFY autoReconnectChanged)
    Q_PROPERTY(bool restrictSubnet READ restrictSubnet WRITE setRestrictSubnet NOTIFY restrictSubnetChanged)

    // Sources / audio
    Q_PROPERTY(bool captureSystemAudio READ captureSystemAudio WRITE setCaptureSystemAudio NOTIFY captureSystemAudioChanged)
    Q_PROPERTY(int appCapture READ appCapture WRITE setAppCapture NOTIFY appCaptureChanged)
    Q_PROPERTY(int sampleRate READ sampleRate WRITE setSampleRate NOTIFY sampleRateChanged)
    Q_PROPERTY(int bufferSize READ bufferSize WRITE setBufferSize NOTIFY bufferSizeChanged)
    Q_PROPERTY(int channels READ channels WRITE setChannels NOTIFY channelsChanged)
    Q_PROPERTY(bool noiseGate READ noiseGate WRITE setNoiseGate NOTIFY noiseGateChanged)
    Q_PROPERTY(bool monitorMute READ monitorMute WRITE setMonitorMute NOTIFY monitorMuteChanged)

    // Optimization (right panel)
    Q_PROPERTY(bool adaptiveBitrate READ adaptiveBitrate WRITE setAdaptiveBitrate NOTIFY adaptiveBitrateChanged)
    Q_PROPERTY(bool adaptiveResolution READ adaptiveResolution WRITE setAdaptiveResolution NOTIFY adaptiveResolutionChanged)
    Q_PROPERTY(bool lowLightBoost READ lowLightBoost WRITE setLowLightBoost NOTIFY lowLightBoostChanged)
    Q_PROPERTY(bool noiseReduction READ noiseReduction WRITE setNoiseReduction NOTIFY noiseReductionChanged)
    Q_PROPERTY(bool mirrorImage READ mirrorImage WRITE setMirrorImage NOTIFY mirrorImageChanged)
    Q_PROPERTY(int colorProfile READ colorProfile WRITE setColorProfile NOTIFY colorProfileChanged)
    Q_PROPERTY(int audioSampleRate READ audioSampleRate WRITE setAudioSampleRate NOTIFY audioSampleRateChanged)

public:
    explicit SettingsViewModel(Settings *backend, QObject *parent = nullptr);

    QString language() const;
    void setLanguage(const QString &v);

    bool darkTheme() const;
    void setDarkTheme(bool v);
    QString accentColor() const;
    void setAccentColor(const QString &v);
    bool telemetryOverlay() const;
    void setTelemetryOverlay(bool v);
    bool compactSidebar() const;
    void setCompactSidebar(bool v);
    bool launchAtLogin() const;
    void setLaunchAtLogin(bool v);

    bool hardwareAccel() const;
    void setHardwareAccel(bool v);
    bool gpuProcessing() const;
    void setGpuProcessing(bool v);
    int streamProtocol() const;
    void setStreamProtocol(int v);
    int encoderPreset() const;
    void setEncoderPreset(int v);
    int maxResolution() const;
    void setMaxResolution(int v);
    int keyframeInterval() const;
    void setKeyframeInterval(int v);
    int bufferedFrames() const;
    void setBufferedFrames(int v);

    bool autoDiscovery() const;
    void setAutoDiscovery(bool v);
    int listenPort() const;
    void setListenPort(int v);
    bool autoReconnect() const;
    void setAutoReconnect(bool v);
    bool restrictSubnet() const;
    void setRestrictSubnet(bool v);

    bool captureSystemAudio() const;
    void setCaptureSystemAudio(bool v);
    int appCapture() const;
    void setAppCapture(int v);
    int sampleRate() const;
    void setSampleRate(int v);
    int bufferSize() const;
    void setBufferSize(int v);
    int channels() const;
    void setChannels(int v);
    bool noiseGate() const;
    void setNoiseGate(bool v);
    bool monitorMute() const;
    void setMonitorMute(bool v);

    bool adaptiveBitrate() const;
    void setAdaptiveBitrate(bool v);
    bool adaptiveResolution() const;
    void setAdaptiveResolution(bool v);
    bool lowLightBoost() const;
    void setLowLightBoost(bool v);
    bool noiseReduction() const;
    void setNoiseReduction(bool v);
    bool mirrorImage() const;
    void setMirrorImage(bool v);
    int colorProfile() const;
    void setColorProfile(int v);
    int audioSampleRate() const;
    void setAudioSampleRate(int v);

signals:
    void languageChanged();
    void darkThemeChanged();
    void accentColorChanged();
    void telemetryOverlayChanged();
    void compactSidebarChanged();
    void launchAtLoginChanged();
    void hardwareAccelChanged();
    void gpuProcessingChanged();
    void streamProtocolChanged();
    void encoderPresetChanged();
    void maxResolutionChanged();
    void keyframeIntervalChanged();
    void bufferedFramesChanged();
    void autoDiscoveryChanged();
    void listenPortChanged();
    void autoReconnectChanged();
    void restrictSubnetChanged();
    void captureSystemAudioChanged();
    void appCaptureChanged();
    void sampleRateChanged();
    void bufferSizeChanged();
    void channelsChanged();
    void noiseGateChanged();
    void monitorMuteChanged();
    void adaptiveBitrateChanged();
    void adaptiveResolutionChanged();
    void lowLightBoostChanged();
    void noiseReductionChanged();
    void mirrorImageChanged();
    void colorProfileChanged();
    void audioSampleRateChanged();

private:
    void applyLaunchAtLogin(bool enable);
    Settings *m_s;
};
