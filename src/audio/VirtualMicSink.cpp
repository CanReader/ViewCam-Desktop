#include "audio/VirtualMicSink.h"
#include "core/Logger.h"

#ifdef __linux__
#include <QDir>
#include <QProcess>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#ifdef VIEWCAM_HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#endif

namespace {
constexpr const char *kSourceName = "viewcam_mic";
// Jitter budget — sized for ZERO audible cuts, which outranks latency. Mic
// chunks share the Wi-Fi socket with video frames, so they arrive in bursts
// (a video frame can hold the link ~25 ms, a TCP retransmit far longer).
//  - prime: buffered before (re)starting playback after start/underrun.
//  - cap:   absolute ceiling; hard drop-oldest happens only while IDLE
//           (nobody recording). While live, excess is shaved one sample per
//           callback (~1 ms/s) — inaudible, unlike a chopped word.
constexpr int kMaxQueuedMs = 250;
constexpr int kPrimeMs = 80;
} // namespace

VirtualMicSink::VirtualMicSink(QObject *parent) : QObject(parent) {
    VC_DEBUG("VirtualMicSink created");
}

VirtualMicSink::~VirtualMicSink() { close(); }

// ─────────────────────────── native PipeWire node ───────────────────────────
#ifdef VIEWCAM_HAVE_PIPEWIRE

namespace {
pw_stream_events makeStreamEvents() {
    pw_stream_events ev{};
    ev.version = PW_VERSION_STREAM_EVENTS;
    ev.process = [](void *ud) { /* replaced below */ };
    return ev;
}
} // namespace

void VirtualMicSink::onStateChanged(void *userData, int, int newState,
                                    const char *error) {
    auto *self = static_cast<VirtualMicSink *>(userData);
    self->m_streamState = newState;
    if (error)
        VC_WARN("ViewCam Microphone stream error: {}", error);
    // A consumer just started pulling: shed the idle backlog down to the
    // steady-state buffer, so recording begins near-live instead of
    // inheriting the full idle cap, and re-prime for a clean start.
    if (newState == PW_STREAM_STATE_STREAMING) {
        QMutexLocker lock(&self->m_ringLock);
        const int keep = self->m_maxRingBytes / 2;
        if (self->m_ring.size() > keep)
            self->m_ring.remove(0, self->m_ring.size() - keep);
        self->m_priming = true;
    }
    pw_thread_loop_signal(self->m_pwLoop, false);
}

// RT thread: hand the graph whatever the phone delivered; silence-pad the
// rest so the node's clock never starves even when the phone is quiet.
void VirtualMicSink::onProcess(void *userData) {
    auto *self = static_cast<VirtualMicSink *>(userData);
    pw_buffer *b = pw_stream_dequeue_buffer(self->m_pwStream);
    if (!b) return;
    spa_data &d = b->buffer->datas[0];
    auto *dst = static_cast<uint8_t *>(d.data);
    if (dst) {
        uint32_t want = d.maxsize;
        if (b->requested > 0)
            want = std::min<uint64_t>(b->requested * self->m_stride, d.maxsize);
        int filled = 0;
        {
            QMutexLocker lock(&self->m_ringLock);
            // Priming: after start/underrun, output silence until kPrimeMs is
            // buffered — one clean gap instead of a burst of micro-cuts.
            if (self->m_priming && self->m_ring.size() >= self->m_primeBytes)
                self->m_priming = false;
            if (!self->m_priming) {
                // Latency creep-down: bursts and clock drift slowly raise the
                // buffered amount. Shave ONE frame per callback while above
                // 1.5× prime — a ~1 ms/s correction nobody can hear, instead
                // of a hard drop that cuts the voice.
                if (self->m_ring.size() > self->m_primeBytes * 3 / 2)
                    self->m_ring.remove(0, self->m_stride);
                filled = std::min<int>(int(want), self->m_ring.size());
                if (filled > 0) {
                    std::memcpy(dst, self->m_ring.constData(), size_t(filled));
                    self->m_ring.remove(0, filled);
                }
                if (uint32_t(filled) < want) {
                    self->m_priming = true; // underrun — refill before resuming
                    self->m_underruns.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        if (uint32_t(filled) < want)
            std::memset(dst + filled, 0, want - uint32_t(filled));
        d.chunk->offset = 0;
        d.chunk->stride = self->m_stride;
        d.chunk->size = want;
    }
    pw_stream_queue_buffer(self->m_pwStream, b);
}

bool VirtualMicSink::openNative(int sampleRate, int channels) {
    pw_init(nullptr, nullptr);

    m_pwLoop = pw_thread_loop_new("viewcam-mic", nullptr);
    if (!m_pwLoop) return false;
    if (pw_thread_loop_start(m_pwLoop) < 0) {
        pw_thread_loop_destroy(m_pwLoop);
        m_pwLoop = nullptr;
        return false;
    }

    static pw_stream_events events = makeStreamEvents();
    events.process = &VirtualMicSink::onProcess;
    events.state_changed = [](void *ud, pw_stream_state o, pw_stream_state n,
                              const char *e) {
        onStateChanged(ud, int(o), int(n), e);
    };

    pw_thread_loop_lock(m_pwLoop);
    pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_CLASS, "Audio/Source",
        PW_KEY_MEDIA_ROLE, "Communication",
        PW_KEY_NODE_NAME, kSourceName,
        PW_KEY_NODE_DESCRIPTION, "ViewCam Microphone",
        // Never auto-picked over a real mic (WirePlumber retargets untargeted
        // capture streams) — users select "ViewCam Microphone" explicitly.
        "priority.session", "0",
        nullptr);
    m_pwStream = pw_stream_new_simple(pw_thread_loop_get_loop(m_pwLoop),
                                      "ViewCam Microphone", props, &events, this);
    if (!m_pwStream) {
        pw_thread_loop_unlock(m_pwLoop);
        closeNative();
        return false;
    }

    spa_audio_info_raw info{};
    info.format = SPA_AUDIO_FORMAT_S16_LE;
    info.rate = uint32_t(sampleRate);
    info.channels = uint32_t(channels);
    uint8_t podBuf[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuf, sizeof(podBuf));
    const spa_pod *params[1] = {
        spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};

    m_streamState = PW_STREAM_STATE_CONNECTING;
    const int res = pw_stream_connect(
        m_pwStream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
        pw_stream_flags(PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
        params, 1);
    if (res >= 0) {
        // No PipeWire daemon (pure PulseAudio) surfaces as an async error —
        // wait briefly for the stream to reach PAUSED (idle source) so the
        // caller can fall back cleanly instead of exposing a dead device.
        timespec deadline{};
        pw_thread_loop_get_time(m_pwLoop, &deadline, 2 * SPA_NSEC_PER_SEC);
        while (m_streamState != PW_STREAM_STATE_ERROR &&
               m_streamState < PW_STREAM_STATE_PAUSED) {
            if (pw_thread_loop_timed_wait_full(m_pwLoop, &deadline) < 0) break;
        }
    }
    pw_thread_loop_unlock(m_pwLoop);

    if (res < 0 || m_streamState == PW_STREAM_STATE_ERROR ||
        m_streamState < PW_STREAM_STATE_PAUSED) {
        closeNative();
        return false;
    }
    return true;
}

void VirtualMicSink::closeNative() {
    if (m_pwLoop) {
        pw_thread_loop_lock(m_pwLoop);
        if (m_pwStream) {
            pw_stream_destroy(m_pwStream);
            m_pwStream = nullptr;
        }
        pw_thread_loop_unlock(m_pwLoop);
        pw_thread_loop_stop(m_pwLoop);
        pw_thread_loop_destroy(m_pwLoop);
        m_pwLoop = nullptr;
    }
    m_native = false;
}

#else

bool VirtualMicSink::openNative(int, int) { return false; }
void VirtualMicSink::closeNative() {}
void VirtualMicSink::onProcess(void *) {}
void VirtualMicSink::onStateChanged(void *, int, int, const char *) {}

#endif // VIEWCAM_HAVE_PIPEWIRE

// ───────────────────────────── shared entry points ──────────────────────────

#ifdef __linux__

bool VirtualMicSink::open(int sampleRate, int channels) {
    if (m_open) return true;

    m_stride = channels * 2;
    m_maxRingBytes = sampleRate * m_stride / 1000 * kMaxQueuedMs;
    m_primeBytes = sampleRate * m_stride / 1000 * kPrimeMs;
    {
        QMutexLocker lock(&m_ringLock);
        m_ring.clear();
        m_priming = true;
    }

    // A crash leaves the previous fallback module (and its FIFO reader)
    // behind; clear it regardless of which backend we're about to use.
    unloadOurModules();

    if (openNative(sampleRate, channels)) {
        m_native = true;
        m_open = true;
        VC_INFO("Virtual microphone active: native PipeWire node '{}'", kSourceName);
        return true;
    }
    return openPipeFallback(sampleRate, channels);
}

bool VirtualMicSink::openPipeFallback(int sampleRate, int channels) {
    const QString runtimeDir =
        qEnvironmentVariable("XDG_RUNTIME_DIR", QDir::tempPath());
    m_fifoPath = runtimeDir + QStringLiteral("/viewcam-mic.pipe");

    if (!loadModule(sampleRate, channels)) {
        VC_WARN("Virtual microphone unavailable (pactl load-module failed)");
        return false;
    }

    // module-pipe-source created the FIFO and holds the read end, so a
    // non-blocking writer opens immediately.
    m_fd = ::open(m_fifoPath.toLocal8Bit().constData(), O_WRONLY | O_NONBLOCK);
    if (m_fd < 0) {
        VC_WARN("Virtual microphone FIFO open failed: {} ({})",
                m_fifoPath.toStdString(), errno);
        close();
        return false;
    }

    // Cap the pipe near the ring budget. The source only drains the FIFO
    // while an app records from it; with the default 64 KB pipe, audio
    // written before that piles up and then plays ~680 ms late FOREVER once
    // recording starts.
#ifdef F_SETPIPE_SZ
    if (::fcntl(m_fd, F_SETPIPE_SZ, 8192) < 0)
        VC_DEBUG("F_SETPIPE_SZ failed ({}) — default pipe size stays", errno);
#endif

    m_open = true;
    VC_INFO("Virtual microphone active: pipe source '{}' (module {}), fifo {}",
            kSourceName, m_moduleId.toStdString(), m_fifoPath.toStdString());
    return true;
}

bool VirtualMicSink::loadModule(int sampleRate, int channels) {
    // The default input must survive our source appearing: WirePlumber both
    // promotes new sources to default AND retargets capture streams that have
    // no pinned target (e.g. a noise-cancel filter chain silently switching
    // its input to viewcam_mic, killing the real mic).
    QProcess q;
    q.start(QStringLiteral("pactl"), {QStringLiteral("get-default-source")});
    q.waitForFinished(3000);
    const QString prevDefault =
        QString::fromLocal8Bit(q.readAllStandardOutput()).trimmed();

    QProcess p;
    p.start(QStringLiteral("pactl"),
            {QStringLiteral("load-module"),
             QStringLiteral("module-pipe-source"),
             QStringLiteral("source_name=%1").arg(QLatin1String(kSourceName)),
             QStringLiteral("file=%1").arg(m_fifoPath),
             QStringLiteral("format=s16le"),
             QStringLiteral("rate=%1").arg(sampleRate),
             QStringLiteral("channels=%1").arg(channels),
             // Backslash-escaped spaces are the one proplist quoting form that
             // survives pactl's modargs AND applies both pairs (verified on
             // pipewire-pulse 1.6.8). priority.session=0 keeps WirePlumber
             // from ever auto-picking this source over a real microphone —
             // users select "ViewCam Microphone" explicitly in their app.
             QStringLiteral("source_properties=device.description=\"ViewCam\\ Microphone\"\\ priority.session=0")});
    if (!p.waitForFinished(5000) || p.exitStatus() != QProcess::NormalExit ||
        p.exitCode() != 0) {
        VC_WARN("pactl load-module failed: {}",
                QString::fromLocal8Bit(p.readAllStandardError()).trimmed().toStdString());
        return false;
    }
    m_moduleId = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
    if (m_moduleId.isEmpty()) return false;

    // Belt and braces: if the session manager switched the default input to
    // us anyway, put the user's device back.
    if (!prevDefault.isEmpty() && prevDefault != QLatin1String(kSourceName)) {
        QProcess d;
        d.start(QStringLiteral("pactl"), {QStringLiteral("get-default-source")});
        d.waitForFinished(3000);
        if (QString::fromLocal8Bit(d.readAllStandardOutput()).trimmed() ==
            QLatin1String(kSourceName)) {
            VC_INFO("Restoring default input to '{}'", prevDefault.toStdString());
            QProcess::execute(QStringLiteral("pactl"),
                              {QStringLiteral("set-default-source"), prevDefault});
        }
    }
    return true;
}

void VirtualMicSink::unloadOurModules() {
    QProcess list;
    list.start(QStringLiteral("pactl"),
               {QStringLiteral("list"), QStringLiteral("short"), QStringLiteral("modules")});
    if (!list.waitForFinished(5000)) return;
    const QString out = QString::fromLocal8Bit(list.readAllStandardOutput());
    for (const QString &line : out.split(QLatin1Char('\n'))) {
        if (!line.contains(QLatin1String("module-pipe-source")) ||
            !line.contains(QLatin1String(kSourceName)))
            continue;
        const QString id = line.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (id.isEmpty()) continue;
        VC_INFO("Unloading stale viewcam_mic module {}", id.toStdString());
        QProcess::execute(QStringLiteral("pactl"),
                          {QStringLiteral("unload-module"), id});
    }
}

void VirtualMicSink::close() {
    if (m_native) closeNative();
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
    if (!m_moduleId.isEmpty()) {
        QProcess::execute(QStringLiteral("pactl"),
                          {QStringLiteral("unload-module"), m_moduleId});
        m_moduleId.clear();
    }
    {
        QMutexLocker lock(&m_ringLock);
        m_ring.clear();
    }
    m_open = false;
}

void VirtualMicSink::writeAudio(const QByteArray &pcm) {
    if (!m_open || pcm.isEmpty()) return;

    if (m_native) {
        // Off the RT thread: surface underruns (each one is an audible gap
        // the LISTENER got — if the log is quiet but a recording has cuts,
        // the cuts arrived inside the phone's stream itself).
        const int u = m_underruns.load(std::memory_order_relaxed);
        if (u != m_underrunsLogged) {
            VC_INFO("Virtual microphone underruns so far: {}", u);
            m_underrunsLogged = u;
        }
        // Cap enforcement (drop-oldest) matters while nobody records — the
        // graph isn't pulling and idle time must not become latency. While
        // live the ring hovers near kPrimeMs and the cap is never reached
        // (creep-down in onProcess handles slow growth inaudibly).
        QMutexLocker lock(&m_ringLock);
        m_ring.append(pcm);
        if (m_ring.size() > m_maxRingBytes)
            m_ring.remove(0, m_ring.size() - m_maxRingBytes);
        return;
    }

    if (m_fd < 0) return;
    // Fallback pipe: same idea, enforced with FIONREAD backpressure — skip
    // the chunk while >40 ms is queued so the reader converges to live.
    int queued = 0;
    if (::ioctl(m_fd, FIONREAD, &queued) == 0 && queued > m_maxRingBytes)
        return;
    const ssize_t n = ::write(m_fd, pcm.constData(), size_t(pcm.size()));
    if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        VC_TRACE("Virtual microphone write error ({})", errno);
}

#elif defined(_WIN32)
// Windows has no user-mode API for creating a virtual microphone device —
// that's a kernel driver's job (which is why DroidCam ships one). The
// pragmatic bridge every tool offers instead: if the free VB-CABLE driver is
// installed, render the phone mic into its "CABLE Input" endpoint; apps then
// select "CABLE Output" as their microphone. Without VB-CABLE, open() fails
// and the mic is preview/meter-only on Windows.

#include "windows/WasapiUtil.h"
#include <atomic>
#include <thread>

namespace {
struct WinMicBridge {
    std::thread thread;
    std::atomic<bool> run{false};
};
} // namespace

bool VirtualMicSink::open(int sampleRate, int channels) {
    if (m_open) return true;
    m_stride = channels * 2;
    m_maxRingBytes = sampleRate * m_stride / 1000 * kMaxQueuedMs;
    m_primeBytes = sampleRate * m_stride / 1000 * kPrimeMs;
    {
        QMutexLocker lock(&m_ringLock);
        m_ring.clear();
        m_priming = true;
    }

    // Find any installed cable-style driver (VB-CABLE, Voicemeeter, VAC).
    vcwin::ComPtr<IMMDevice> cable;
    {
        vcwin::ScopedCom com;
        if (!com.ok) return false;
        const vcwin::CablePair pair = vcwin::findCablePair();
        cable = pair.render;
        m_deviceName = pair.captureName;
        if (cable)
            VC_INFO("Virtual mic bridge: rendering into '{}', apps record from '{}'",
                    pair.renderName.toStdString(), pair.captureName.toStdString());
    }
    if (!cable) {
        VC_WARN("Virtual microphone unavailable: no virtual-cable driver "
                "(install the free VB-CABLE, vb-audio.com/Cable) — mic stays "
                "meter-only on Windows");
        return false;
    }

    auto *state = new WinMicBridge();
    state->run = true;
    m_win = state;
    const int srcRate = sampleRate;
    const int srcCh = channels;

    state->thread = std::thread([this, state, cable, srcRate, srcCh]() {
        vcwin::ScopedCom com;
        vcwin::ComPtr<IAudioClient> client;
        vcwin::ComPtr<IAudioRenderClient> render;
        WAVEFORMATEX *wf = nullptr;
        bool ok = com.ok &&
            SUCCEEDED(cable->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                      reinterpret_cast<void **>(client.GetAddressOf()))) &&
            SUCCEEDED(client->GetMixFormat(&wf)) &&
            SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0,
                                         2000000 /* 200 ms */, 0, wf, nullptr)) &&
            SUCCEEDED(client->GetService(__uuidof(IAudioRenderClient),
                                        reinterpret_cast<void **>(render.GetAddressOf()))) &&
            SUCCEEDED(client->Start());
        if (!ok) {
            VC_WARN("VB-CABLE render init failed — virtual microphone unavailable");
            if (wf) CoTaskMemFree(wf);
            return;
        }
        const vcwin::MixFormat fmt = vcwin::MixFormat::from(wf);
        UINT32 bufferFrames = 0;
        client->GetBufferSize(&bufferFrames);
        CoTaskMemFree(wf);
        VC_INFO("Virtual microphone active: VB-CABLE bridge ({} Hz device)", fmt.rate);

        // Naive linear resampler position (mono source), in source frames.
        double srcPos = 0.0;
        const double step = double(srcRate) / double(fmt.rate);

        while (state->run) {
            Sleep(10);
            UINT32 padding = 0;
            if (FAILED(client->GetCurrentPadding(&padding))) break;
            UINT32 want = bufferFrames - padding;
            // Cap each write at ~50 ms so a hiccup can't queue huge latency.
            const UINT32 cap = UINT32(fmt.rate / 20);
            if (want > cap) want = cap;
            if (want == 0) continue;
            BYTE *dst = nullptr;
            if (FAILED(render->GetBuffer(want, &dst))) break;

            QMutexLocker lock(&m_ringLock);
            const auto *src = reinterpret_cast<const int16_t *>(m_ring.constData());
            const int srcFrames = m_ring.size() / m_stride;
            UINT32 produced = 0;
            for (; produced < want; ++produced) {
                const int idx = int(srcPos);
                if (idx + 1 >= srcFrames) break; // ring exhausted
                const float a = float(src[idx * srcCh]) / 32768.0f;
                const float b = float(src[(idx + 1) * srcCh]) / 32768.0f;
                const float frac = float(srcPos - double(idx));
                const float v = a + (b - a) * frac;
                uint8_t *frame = dst + size_t(produced) * fmt.frameBytes;
                for (int c = 0; c < fmt.channels; ++c)
                    vcwin::writeSample(frame, fmt, c, v);
                srcPos += step;
            }
            // Drop the consumed source frames; keep the fractional remainder.
            const int consumed = int(srcPos);
            if (consumed > 0) {
                m_ring.remove(0, qMin(consumed * m_stride, m_ring.size()));
                srcPos -= double(consumed);
            }
            lock.unlock();
            // Silence-pad whatever the ring couldn't fill.
            for (; produced < want; ++produced) {
                uint8_t *frame = dst + size_t(produced) * fmt.frameBytes;
                for (int c = 0; c < fmt.channels; ++c)
                    vcwin::writeSample(frame, fmt, c, 0.0f);
            }
            render->ReleaseBuffer(want, 0);
        }
        client->Stop();
    });

    m_open = true;
    return true;
}

void VirtualMicSink::close() {
    auto *state = static_cast<WinMicBridge *>(m_win);
    if (state) {
        state->run = false;
        if (state->thread.joinable()) state->thread.join();
        delete state;
        m_win = nullptr;
    }
    {
        QMutexLocker lock(&m_ringLock);
        m_ring.clear();
    }
    m_open = false;
}

void VirtualMicSink::writeAudio(const QByteArray &pcm) {
    if (!m_open || pcm.isEmpty()) return;
    QMutexLocker lock(&m_ringLock);
    m_ring.append(pcm);
    if (m_ring.size() > m_maxRingBytes)
        m_ring.remove(0, m_ring.size() - m_maxRingBytes);
}

bool VirtualMicSink::openPipeFallback(int, int) { return false; }
bool VirtualMicSink::loadModule(int, int) { return false; }
void VirtualMicSink::unloadOurModules() {}

#else // other platforms — no audio backend

bool VirtualMicSink::open(int, int) {
    VC_WARN("Virtual microphone not supported on this platform yet");
    return false;
}
bool VirtualMicSink::openPipeFallback(int, int) { return false; }
bool VirtualMicSink::loadModule(int, int) { return false; }
void VirtualMicSink::close() { m_open = false; }
void VirtualMicSink::writeAudio(const QByteArray &) {}
void VirtualMicSink::unloadOurModules() {}

#endif
