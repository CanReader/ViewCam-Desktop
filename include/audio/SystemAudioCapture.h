#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QProcess;
class Settings;

// Captures what this computer is playing as interleaved PCM s16le, for the
// phone-as-speaker return feed. Linux only for now: spawns `parec`, which
// works on PulseAudio and PipeWire alike with no extra link dependency.
// Emits fixed ~20 ms chunks sized for one wire AUDIO frame (spec §4.1).
//
// Two capture modes:
//  - shared (localPlayback on): taps @DEFAULT_MONITOR@ — the computer's own
//    speakers keep playing.
//  - exclusive (localPlayback off): loads a "ViewCam Output" null sink, makes
//    it the default, moves the current streams into it and taps ITS monitor —
//    the phone becomes the only place sound comes out. Everything is restored
//    on stop; a crash is healed by recoverStaleRouting() next launch.
class SystemAudioCapture : public QObject {
    Q_OBJECT

public:
    explicit SystemAudioCapture(Settings *settings, QObject *parent = nullptr);
    ~SystemAudioCapture() override;

    bool start(int sampleRate, int channels, bool exclusive);
    void stop();
    bool isRunning() const;

    // ACTUAL capture format — may differ from the requested one (Windows
    // captures at the device mix rate, e.g. 44100 Hz). Valid after start().
    int sampleRate() const { return m_rate; }
    int channels() const { return m_channels; }

    // Crash aftermath repair, called once at startup (deferred): if a previous
    // run died while exclusive routing was active, the user's default sink is
    // still the (now orphaned) null sink — i.e. NO sound anywhere. Restores
    // the persisted previous default, moves streams back, unloads leftovers.
    // Touches the audio server ONLY when such leftovers actually exist.
    static void recoverStaleRouting(Settings *settings);

signals:
    void chunkReady(const QByteArray &pcm);

private:
    bool setupExclusiveRouting(int sampleRate);
    void teardownExclusiveRouting();

    Settings *m_settings;
    QProcess *m_proc = nullptr;
    QByteArray m_buffer;
    int m_chunkBytes = 0;
    int m_rate = 0;
    int m_channels = 0;
    QString m_nullModuleId;  // loaded module-null-sink index ("" = shared mode)
    QString m_savedSink;     // default sink to restore after exclusive mode
    void *m_win = nullptr;   // Windows WASAPI state (opaque; audio/windows)
};
