#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QObject>
#include <QtQml/qqmlregistration.h>

class Settings;

// Audio state for the Sources page and the LiveView mic/volume controls.
// Same split as the camera path: this holds UI-facing state, AppController
// owns the wiring to StreamReceiver / VirtualMicSink / SystemAudioCapture.
class AudioViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    // Desktop wants the phone microphone stream (LiveView mic button). The
    // phone only sends AUDIO frames while this is true (CONTROL micEnabled).
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled NOTIFY micEnabledChanged)
    // Desktop is streaming its system audio to the phone (Sources page
    // "Capture system audio"; runtime mirror of that persisted toggle).
    Q_PROPERTY(bool speakerEnabled READ speakerEnabled WRITE setSpeakerEnabled NOTIFY speakerEnabledChanged)
    // The phone confirmed (STATUS controls{}) that it is actually capturing /
    // playing — permission granted, recorder or player live.
    Q_PROPERTY(bool micActive READ micActive NOTIFY micActiveChanged)
    Q_PROPERTY(bool speakerActive READ speakerActive NOTIFY speakerActiveChanged)
    // False when the phone reported RECORD_AUDIO is not granted — the UI can
    // then say WHY the mic is silent instead of "waiting" forever.
    Q_PROPERTY(bool micPermission READ micPermission NOTIFY micPermissionChanged)
    // The connected phone can do audio at all (HELLO "audio":true, ≥1.3.0).
    Q_PROPERTY(bool phoneAudioCapable READ phoneAudioCapable NOTIFY phoneAudioCapableChanged)
    // The desktop-side virtual microphone opened (Linux: pipe source loaded).
    Q_PROPERTY(bool virtualMicReady READ virtualMicReady NOTIFY virtualMicReadyChanged)
    // System-audio capture actually running (false + speakerEnabled = this
    // computer has no output device to capture — zero-speaker PC guidance).
    Q_PROPERTY(bool speakerCaptureRunning READ speakerCaptureRunning NOTIFY speakerCaptureRunningChanged)
    // Smoothed mic level 0..1 from real received PCM — drives the meters.
    Q_PROPERTY(double micLevel READ micLevel NOTIFY micLevelChanged)

public:
    explicit AudioViewModel(Settings *settings, QObject *parent = nullptr);

    bool micEnabled() const { return m_micEnabled; }
    void setMicEnabled(bool on);
    bool speakerEnabled() const { return m_speakerEnabled; }
    void setSpeakerEnabled(bool on);
    bool micActive() const { return m_micActive; }
    bool speakerActive() const { return m_speakerActive; }
    bool micPermission() const { return m_micPermission; }
    bool phoneAudioCapable() const { return m_phoneAudioCapable; }
    bool virtualMicReady() const { return m_virtualMicReady; }
    bool speakerCaptureRunning() const { return m_speakerCaptureRunning; }
    void setSpeakerCaptureRunning(bool running);
    double micLevel() const { return m_micLevel; }

    // Driven by AppController
    void setPhoneAudioCapable(bool capable);
    void setVirtualMicReady(bool ready);
    // "mic"/"speaker" echo from STATUS controls{} (absent keys = unchanged).
    void applyControlEcho(const QJsonObject &controls);
    // Feed a received mic PCM chunk (s16le) into the level meter.
    void reportMicChunk(const QByteArray &pcm);
    void resetLevel();

signals:
    void micEnabledChanged();
    void speakerEnabledChanged();
    void micActiveChanged();
    void speakerActiveChanged();
    void micPermissionChanged();
    void phoneAudioCapableChanged();
    void virtualMicReadyChanged();
    void speakerCaptureRunningChanged();
    void micLevelChanged();

private:
    void setMicActive(bool on);
    void setSpeakerActive(bool on);

    Settings *m_settings;
    bool m_micEnabled = false;
    bool m_speakerEnabled = false;
    bool m_micActive = false;
    bool m_speakerActive = false;
    bool m_micPermission = true; // assume granted until the phone says otherwise
    bool m_phoneAudioCapable = false;
    bool m_virtualMicReady = false;
    bool m_speakerCaptureRunning = false;
    double m_micLevel = 0.0;
    QElapsedTimer m_levelNotify; // throttles micLevelChanged to UI rates
};
