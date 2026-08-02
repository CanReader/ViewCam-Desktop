#include "viewmodels/AudioViewModel.h"
#include "core/Logger.h"
#include "core/Settings.h"

#include <cmath>

namespace {
const auto kMicEnabledKey = QStringLiteral("sources/micEnabled");
} // namespace

AudioViewModel::AudioViewModel(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    // Off by default, like the speaker: audio only flows after the user
    // explicitly turns it on (no surprise hot mic on first connect).
    m_micEnabled = m_settings->value(kMicEnabledKey, false).toBool();
    m_levelNotify.start();
}

void AudioViewModel::setMicEnabled(bool on) {
    if (m_micEnabled == on) return;
    m_micEnabled = on;
    m_settings->setValue(kMicEnabledKey, on);
    VC_INFO("Phone microphone {}", on ? "enabled" : "muted");
    emit micEnabledChanged();
}

void AudioViewModel::setSpeakerEnabled(bool on) {
    if (m_speakerEnabled == on) return;
    m_speakerEnabled = on;
    VC_INFO("System audio capture {}", on ? "enabled" : "disabled");
    emit speakerEnabledChanged();
}

void AudioViewModel::setPhoneAudioCapable(bool capable) {
    if (m_phoneAudioCapable == capable) return;
    m_phoneAudioCapable = capable;
    emit phoneAudioCapableChanged();
}

void AudioViewModel::setVirtualMicReady(bool ready) {
    if (m_virtualMicReady == ready) return;
    m_virtualMicReady = ready;
    emit virtualMicReadyChanged();
}

void AudioViewModel::setSpeakerCaptureRunning(bool running) {
    if (m_speakerCaptureRunning == running) return;
    m_speakerCaptureRunning = running;
    emit speakerCaptureRunningChanged();
}

void AudioViewModel::applyControlEcho(const QJsonObject &controls) {
    if (controls.contains(QStringLiteral("mic")))
        setMicActive(controls.value(QStringLiteral("mic")).toBool(false));
    if (controls.contains(QStringLiteral("speaker")))
        setSpeakerActive(controls.value(QStringLiteral("speaker")).toBool(false));
    if (controls.contains(QStringLiteral("micPermission"))) {
        const bool granted =
            controls.value(QStringLiteral("micPermission")).toBool(true);
        if (granted != m_micPermission) {
            m_micPermission = granted;
            emit micPermissionChanged();
        }
    }
}

void AudioViewModel::setMicActive(bool on) {
    if (m_micActive == on) return;
    m_micActive = on;
    emit micActiveChanged();
    if (!on) resetLevel();
}

void AudioViewModel::setSpeakerActive(bool on) {
    if (m_speakerActive == on) return;
    m_speakerActive = on;
    emit speakerActiveChanged();
}

void AudioViewModel::reportMicChunk(const QByteArray &pcm) {
    const auto *samples = reinterpret_cast<const qint16 *>(pcm.constData());
    const int n = int(pcm.size() / 2);
    if (n == 0) return;

    double acc = 0.0;
    for (int i = 0; i < n; ++i)
        acc += double(samples[i]) * double(samples[i]);
    // sqrt of RMS lifts quiet-but-audible speech into the meter's visible
    // range (linear RMS of speech sits at ~0.02-0.1 and the bars barely move).
    const double level =
        std::min(1.0, std::sqrt(std::sqrt(acc / n) / 32768.0) * 1.35);

    // Fast attack, slow decay — the standard meter ballistics.
    m_micLevel = level > m_micLevel ? level : m_micLevel * 0.82;
    if (m_micLevel < 0.004) m_micLevel = 0.0;

    if (m_levelNotify.elapsed() >= 33) { // ≤30 Hz into QML bindings
        m_levelNotify.restart();
        emit micLevelChanged();
    }
}

void AudioViewModel::resetLevel() {
    if (m_micLevel == 0.0) return;
    m_micLevel = 0.0;
    emit micLevelChanged();
}
