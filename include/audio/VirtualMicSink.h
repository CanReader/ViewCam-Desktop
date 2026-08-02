#pragma once

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QString>
#include <atomic>

struct pw_thread_loop;
struct pw_stream;

// Virtual microphone device the phone's audio is written into. On Linux this
// is, preferably, a NATIVE PipeWire stream node ("ViewCam Microphone",
// media.class Audio/Source): the app pushes samples straight into the graph —
// no FIFO, no stale kernel-side backlog, ~one quantum of latency, and the
// node vanishes with the process (crash-safe by construction). On systems
// without a PipeWire daemon (pure PulseAudio) it falls back to a
// module-pipe-source FIFO via pactl. Windows: not yet implemented.
class VirtualMicSink : public QObject {
    Q_OBJECT

public:
    explicit VirtualMicSink(QObject *parent = nullptr);
    ~VirtualMicSink() override;

    // Create the source node (native) or load the pipe-source (fallback).
    // Idempotent; may block briefly — call it deferred, like the vcam open().
    bool open(int sampleRate, int channels);
    void close();
    bool isOpen() const { return m_open; }

    // Unload any viewcam_mic pipe-source a crashed previous run left behind
    // (a writer-less pipe source destabilizes PipeWire). Only relevant to the
    // pactl fallback — a native node can't outlive the process.
    void cleanupStale() { unloadOurModules(); }

public slots:
    // Interleaved PCM s16le at the open() rate/channels. Never blocks; the
    // ring keeps at most ~40 ms and drops OLDEST on overflow, so the source
    // stays live-latency no matter how long nobody was recording from it.
    void writeAudio(const QByteArray &pcm);

private:
    // Native PipeWire stream backend.
    bool openNative(int sampleRate, int channels);
    void closeNative();
    static void onProcess(void *userData);
    static void onStateChanged(void *userData, int oldState, int newState,
                               const char *error);

    // pactl module-pipe-source fallback.
    bool openPipeFallback(int sampleRate, int channels);
    bool loadModule(int sampleRate, int channels);
    void unloadOurModules();

    bool m_open = false;

    // native backend state
    pw_thread_loop *m_pwLoop = nullptr;
    pw_stream *m_pwStream = nullptr;
    QMutex m_ringLock;
    QByteArray m_ring;       // pending PCM, capped at kMaxRingBytes
    int m_stride = 2;        // bytes per frame (channels * 2)
    int m_maxRingBytes = 0;  // jitter cap at the open() rate/channels
    int m_primeBytes = 0;    // min buffered before (re)starting playback
    bool m_priming = true;   // guarded by m_ringLock
    // Diagnostics: bumped on the RT thread per underrun, logged from
    // writeAudio — separates network stalls from phone-side gating.
    std::atomic<int> m_underruns{0};
    int m_underrunsLogged = 0;
    bool m_native = false;
    // guarded by the thread loop's lock (see onStateChanged)
    int m_streamState = 0;

    // fallback backend state
    QString m_fifoPath;
    QString m_moduleId; // pactl module index, for unload on close
    int m_fd = -1;
    void *m_win = nullptr; // Windows WASAPI state (opaque; audio/windows)
};
