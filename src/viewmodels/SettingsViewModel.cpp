#include "viewmodels/SettingsViewModel.h"
#include "core/Constants.h"
#include "core/Settings.h"

// Implementation boilerplate: each property is one QSettings key.
#define VC_IMPL(TYPE, GETTER, SETTER, SIGNAL, KEY, DEFAULT)                    \
  TYPE SettingsViewModel::GETTER() const {                                    \
    return m_s->value(QStringLiteral(KEY), DEFAULT).value<TYPE>();            \
  }                                                                           \
  void SettingsViewModel::SETTER(TYPE v) {                                    \
    if (GETTER() == v)                                                        \
      return;                                                                 \
    m_s->setValue(QStringLiteral(KEY), v);                                    \
    emit SIGNAL();                                                            \
  }

#define VC_IMPL_REF(TYPE, GETTER, SETTER, SIGNAL, KEY, DEFAULT)               \
  TYPE SettingsViewModel::GETTER() const {                                    \
    return m_s->value(QStringLiteral(KEY), DEFAULT).value<TYPE>();            \
  }                                                                           \
  void SettingsViewModel::SETTER(const TYPE &v) {                             \
    if (GETTER() == v)                                                        \
      return;                                                                 \
    m_s->setValue(QStringLiteral(KEY), v);                                    \
    emit SIGNAL();                                                            \
  }

SettingsViewModel::SettingsViewModel(Settings *backend, QObject *parent)
    : QObject(parent), m_s(backend) {}

VC_IMPL(bool, darkTheme, setDarkTheme, darkThemeChanged, "appearance/darkTheme", true)
VC_IMPL_REF(QString, accentColor, setAccentColor, accentColorChanged, "appearance/accent", QStringLiteral("#8B7CFF"))
VC_IMPL(bool, telemetryOverlay, setTelemetryOverlay, telemetryOverlayChanged, "appearance/telemetryOverlay", true)
VC_IMPL(bool, compactSidebar, setCompactSidebar, compactSidebarChanged, "appearance/compactSidebar", false)
VC_IMPL(bool, launchAtLogin, setLaunchAtLogin, launchAtLoginChanged, "appearance/launchAtLogin", false)

VC_IMPL(bool, hardwareAccel, setHardwareAccel, hardwareAccelChanged, "engine/hardwareAccel", true)
VC_IMPL(bool, gpuProcessing, setGpuProcessing, gpuProcessingChanged, "engine/gpuProcessing", true)
VC_IMPL(int, streamProtocol, setStreamProtocol, streamProtocolChanged, "engine/protocol", 0)
VC_IMPL(int, encoderPreset, setEncoderPreset, encoderPresetChanged, "engine/preset", 1)
VC_IMPL(int, maxResolution, setMaxResolution, maxResolutionChanged, "engine/maxResolution", 1)

VC_IMPL(bool, autoDiscovery, setAutoDiscovery, autoDiscoveryChanged, "network/autoDiscovery", true)
VC_IMPL(int, listenPort, setListenPort, listenPortChanged, "connection/port", vc::kStreamPort)
VC_IMPL(bool, autoReconnect, setAutoReconnect, autoReconnectChanged, "network/autoReconnect", true)
VC_IMPL(bool, restrictSubnet, setRestrictSubnet, restrictSubnetChanged, "network/restrictSubnet", true)

VC_IMPL(bool, captureSystemAudio, setCaptureSystemAudio, captureSystemAudioChanged, "sources/captureSystemAudio", true)
VC_IMPL(int, appCapture, setAppCapture, appCaptureChanged, "sources/appCapture", 0)
VC_IMPL(int, sampleRate, setSampleRate, sampleRateChanged, "sources/sampleRate", 1)
VC_IMPL(int, bufferSize, setBufferSize, bufferSizeChanged, "sources/bufferSize", 1)
VC_IMPL(int, channels, setChannels, channelsChanged, "sources/channels", 1)
VC_IMPL(bool, noiseGate, setNoiseGate, noiseGateChanged, "sources/noiseGate", true)
VC_IMPL(bool, monitorMute, setMonitorMute, monitorMuteChanged, "sources/monitorMute", false)

VC_IMPL(bool, adaptiveBitrate, setAdaptiveBitrate, adaptiveBitrateChanged, "optimize/adaptiveBitrate", true)
VC_IMPL(bool, adaptiveResolution, setAdaptiveResolution, adaptiveResolutionChanged, "optimize/adaptiveResolution", true)
VC_IMPL(bool, lowLightBoost, setLowLightBoost, lowLightBoostChanged, "optimize/lowLightBoost", false)
VC_IMPL(bool, noiseReduction, setNoiseReduction, noiseReductionChanged, "optimize/noiseReduction", true)
VC_IMPL(bool, mirrorImage, setMirrorImage, mirrorImageChanged, "optimize/mirrorImage", false)
VC_IMPL(int, colorProfile, setColorProfile, colorProfileChanged, "optimize/colorProfile", 0)
VC_IMPL(int, audioSampleRate, setAudioSampleRate, audioSampleRateChanged, "optimize/audioSampleRate", 1)
