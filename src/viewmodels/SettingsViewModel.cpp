#include "viewmodels/SettingsViewModel.h"
#include "core/Constants.h"
#include "core/Settings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>
#if defined(Q_OS_WIN)
#include <QSettings>
#endif

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

VC_IMPL_REF(QString, language, setLanguage, languageChanged, "appearance/language", QStringLiteral(""))
VC_IMPL(bool, darkTheme, setDarkTheme, darkThemeChanged, "appearance/darkTheme", true)
VC_IMPL_REF(QString, accentColor, setAccentColor, accentColorChanged, "appearance/accent", QStringLiteral("#8B7CFF"))
VC_IMPL(bool, telemetryOverlay, setTelemetryOverlay, telemetryOverlayChanged, "appearance/telemetryOverlay", true)
VC_IMPL(bool, compactSidebar, setCompactSidebar, compactSidebarChanged, "appearance/compactSidebar", false)
bool SettingsViewModel::launchAtLogin() const {
    return m_s->value(QStringLiteral("appearance/launchAtLogin"), false).value<bool>();
}

void SettingsViewModel::setLaunchAtLogin(bool v) {
    if (launchAtLogin() == v)
        return;
    m_s->setValue(QStringLiteral("appearance/launchAtLogin"), v);
    emit launchAtLoginChanged();
    applyLaunchAtLogin(v);
}

void SettingsViewModel::applyLaunchAtLogin(bool enable) {
#if defined(Q_OS_LINUX)
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + QStringLiteral("/autostart");
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/ViewCam.desktop");
    if (enable) {
        QFile f(file);
        if (f.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream ts(&f);
            ts << "[Desktop Entry]\n"
               << "Type=Application\n"
               << "Name=ViewCam\n"
               << "Exec=" << QCoreApplication::applicationFilePath() << "\n"
               << "StartupNotify=false\n"
               << "Hidden=false\n";
        }
    } else {
        QFile::remove(file);
    }
#elif defined(Q_OS_MACOS)
    const QString plistDir = QDir::homePath() + QStringLiteral("/Library/LaunchAgents");
    QDir().mkpath(plistDir);
    const QString plist = plistDir + QStringLiteral("/com.viewcam.ViewCam.plist");
    if (enable) {
        QFile f(plist);
        if (f.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream ts(&f);
            const QString exe = QCoreApplication::applicationFilePath();
            ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               << "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"\n"
               << "    \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
               << "<plist version=\"1.0\"><dict>\n"
               << "  <key>Label</key><string>com.viewcam.ViewCam</string>\n"
               << "  <key>ProgramArguments</key><array><string>" << exe << "</string></array>\n"
               << "  <key>RunAtLoad</key><true/>\n"
               << "  <key>KeepAlive</key><false/>\n"
               << "</dict></plist>\n";
        }
    } else {
        QFile::remove(plist);
    }
#elif defined(Q_OS_WIN)
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                  QSettings::NativeFormat);
    if (enable) {
        reg.setValue(QStringLiteral("ViewCam"),
                     QDir::toNativeSeparators(QCoreApplication::applicationFilePath()));
    } else {
        reg.remove(QStringLiteral("ViewCam"));
    }
#else
    Q_UNUSED(enable)
#endif
}

VC_IMPL(bool, hardwareAccel, setHardwareAccel, hardwareAccelChanged, "engine/hardwareAccel", true)
VC_IMPL(bool, gpuProcessing, setGpuProcessing, gpuProcessingChanged, "engine/gpuProcessing", true)
VC_IMPL(int, streamProtocol, setStreamProtocol, streamProtocolChanged, "engine/protocol", 0)
VC_IMPL(int, encoderPreset, setEncoderPreset, encoderPresetChanged, "engine/preset", 1)
VC_IMPL(int, maxResolution, setMaxResolution, maxResolutionChanged, "engine/maxResolution", 1)
VC_IMPL(int, keyframeInterval, setKeyframeInterval, keyframeIntervalChanged, "engine/keyframeInterval", 2)
VC_IMPL(int, bufferedFrames, setBufferedFrames, bufferedFramesChanged, "engine/bufferedFrames", 2)

// Updates: auto-download toggle + periodic check frequency.
// updateFrequency: 0 = On launch, 1 = Daily, 2 = Weekly. Default Daily.
VC_IMPL(bool, autoUpdate, setAutoUpdate, autoUpdateChanged, "updates/autoDownload", true)
VC_IMPL(int, updateFrequency, setUpdateFrequency, updateFrequencyChanged, "updates/frequency", 1)

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
