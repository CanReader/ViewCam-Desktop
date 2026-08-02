#include "audio/SystemAudioCapture.h"
#include "core/Logger.h"
#include "core/Settings.h"

#include <QProcess>

#ifdef __linux__
namespace {
constexpr const char *kNullSinkName = "viewcam_out";
const auto kSavedSinkKey = QStringLiteral("sources/savedDefaultSink");

// Run pactl and return trimmed stdout ("" on any failure).
QString pactl(const QStringList &args) {
    QProcess p;
    p.start(QStringLiteral("pactl"), args);
    if (!p.waitForFinished(5000) || p.exitStatus() != QProcess::NormalExit ||
        p.exitCode() != 0)
        return QString();
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

// Move every current playback stream to [sink]. Best-effort: a stream that
// refuses to move (or vanished mid-loop) is skipped, not fatal.
void moveAllSinkInputsTo(const QString &sink) {
    const QString out = pactl({QStringLiteral("list"), QStringLiteral("short"),
                               QStringLiteral("sink-inputs")});
    for (const QString &line : out.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        const QString id = line.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (!id.isEmpty())
            QProcess::execute(QStringLiteral("pactl"),
                              {QStringLiteral("move-sink-input"), id, sink});
    }
}
} // namespace
#endif

SystemAudioCapture::SystemAudioCapture(Settings *settings, QObject *parent)
    : QObject(parent), m_settings(settings) {
    VC_DEBUG("SystemAudioCapture created");
}

SystemAudioCapture::~SystemAudioCapture() { stop(); }

#ifdef __linux__

bool SystemAudioCapture::isRunning() const {
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

bool SystemAudioCapture::start(int sampleRate, int channels, bool exclusive) {
    if (isRunning()) return true;

    m_rate = sampleRate;
    m_channels = channels;
    // Zero-speaker PCs are a first-class target: with no (or no default)
    // sink there is nothing to monitor, so create our null sink and make it
    // the default — the phone IS this computer's speaker then, and apps
    // finally have an output device to play into.
    bool useOwnSink = exclusive;
    if (!useOwnSink && pactl({QStringLiteral("get-default-sink")}).isEmpty()) {
        VC_INFO("No default output device — routing through ViewCam Output");
        useOwnSink = true;
    }
    const QString device = (useOwnSink && setupExclusiveRouting(sampleRate))
        ? QStringLiteral("%1.monitor").arg(QLatin1String(kNullSinkName))
        : QStringLiteral("@DEFAULT_MONITOR@");

    // 20 ms of interleaved s16le — one wire AUDIO frame per chunk.
    m_chunkBytes = sampleRate / 50 * channels * 2;
    m_buffer.clear();

    m_proc = new QProcess(this);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        m_buffer.append(m_proc->readAllStandardOutput());
        while (m_buffer.size() >= m_chunkBytes) {
            emit chunkReady(m_buffer.left(m_chunkBytes));
            m_buffer.remove(0, m_chunkBytes);
        }
    });
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        VC_WARN("parec failed to run — system audio capture unavailable");
    });
    connect(m_proc, &QProcess::finished, this,
            [](int code, QProcess::ExitStatus) {
                VC_INFO("parec exited ({})", code);
            });

    m_proc->start(QStringLiteral("parec"),
                  {QStringLiteral("--format=s16le"),
                   QStringLiteral("--rate=%1").arg(sampleRate),
                   QStringLiteral("--channels=%1").arg(channels),
                   QStringLiteral("--latency-msec=20"),
                   QStringLiteral("--raw"),
                   QStringLiteral("--device=%1").arg(device)});
    if (!m_proc->waitForStarted(3000)) {
        VC_WARN("System audio capture unavailable (parec not found?)");
        m_proc->deleteLater();
        m_proc = nullptr;
        teardownExclusiveRouting();
        return false;
    }
    VC_INFO("System audio capture started ({} Hz, {} ch, {})", sampleRate,
            channels, m_nullModuleId.isEmpty() ? "shared" : "exclusive");
    return true;
}

void SystemAudioCapture::stop() {
    if (m_proc) {
        VC_INFO("System audio capture stopped");
        m_proc->disconnect(this);
        m_proc->kill();
        m_proc->waitForFinished(1000);
        m_proc->deleteLater();
        m_proc = nullptr;
        m_buffer.clear();
    }
    teardownExclusiveRouting();
}

/**
 * Phone-only mode: sound must stop coming out of this computer while still
 * being capturable. Muting the real sink won't do — its monitor goes silent
 * with it. Instead route everything into a null sink and tap THAT monitor:
 * load "ViewCam Output", make it the default (new streams follow), move the
 * currently-playing streams over, and remember the real sink for restore.
 */
bool SystemAudioCapture::setupExclusiveRouting(int sampleRate) {
    if (!m_nullModuleId.isEmpty()) return true;

    m_savedSink = pactl({QStringLiteral("get-default-sink")});
    // Persisted so a crash mid-session can restore it next launch — otherwise
    // the user is left with a dead null sink as default (silence everywhere).
    m_settings->setValue(kSavedSinkKey, m_savedSink);

    m_nullModuleId = pactl(
        {QStringLiteral("load-module"), QStringLiteral("module-null-sink"),
         QStringLiteral("sink_name=%1").arg(QLatin1String(kNullSinkName)),
         QStringLiteral("rate=%1").arg(sampleRate),
         QStringLiteral("sink_properties=device.description=\"ViewCam Output\"")});
    if (m_nullModuleId.isEmpty()) {
        VC_WARN("Exclusive output unavailable (null-sink load failed) — falling back to shared");
        m_settings->setValue(kSavedSinkKey, QString());
        return false;
    }
    pactl({QStringLiteral("set-default-sink"), QLatin1String(kNullSinkName)});
    moveAllSinkInputsTo(QLatin1String(kNullSinkName));
    VC_INFO("Exclusive output: default sink '{}' -> {} (module {})",
            m_savedSink.toStdString(), kNullSinkName,
            m_nullModuleId.toStdString());
    return true;
}

void SystemAudioCapture::teardownExclusiveRouting() {
    if (m_nullModuleId.isEmpty()) return;
    if (!m_savedSink.isEmpty()) {
        pactl({QStringLiteral("set-default-sink"), m_savedSink});
        moveAllSinkInputsTo(m_savedSink);
    }
    QProcess::execute(QStringLiteral("pactl"),
                      {QStringLiteral("unload-module"), m_nullModuleId});
    VC_INFO("Exclusive output restored to '{}'", m_savedSink.toStdString());
    m_nullModuleId.clear();
    m_savedSink.clear();
    m_settings->setValue(kSavedSinkKey, QString());
}

void SystemAudioCapture::recoverStaleRouting(Settings *settings) {
    const QString mods = pactl({QStringLiteral("list"), QStringLiteral("short"),
                                QStringLiteral("modules")});
    for (const QString &line : mods.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (!line.contains(QLatin1String("module-null-sink")) ||
            !line.contains(QLatin1String(kNullSinkName)))
            continue;
        VC_WARN("Stale ViewCam Output null sink from a previous run — restoring routing");
        const QString saved = settings->value(kSavedSinkKey, QString()).toString();
        if (!saved.isEmpty()) {
            pactl({QStringLiteral("set-default-sink"), saved});
            moveAllSinkInputsTo(saved);
        }
        const QString id = line.section(QLatin1Char('\t'), 0, 0).trimmed();
        if (!id.isEmpty())
            QProcess::execute(QStringLiteral("pactl"),
                              {QStringLiteral("unload-module"), id});
        settings->setValue(kSavedSinkKey, QString());
    }
}

#elif defined(_WIN32)
// WASAPI shared-mode loopback of the default render endpoint: captures what
// the computer plays at the device MIX rate (often 44100 — the caller must
// use sampleRate()/channels(), not what it asked for). Always emitted as
// 2-channel s16le in ~20 ms chunks. No driver required.

#include "audio/windows/WasapiUtil.h"
#include <atomic>
#include <thread>

namespace {
struct WinLoopback {
    std::thread thread;
    std::atomic<bool> run{false};
    vcwin::ComPtr<IMMDevice> device; // endpoint picked at start()
};
} // namespace

bool SystemAudioCapture::isRunning() const { return m_win != nullptr; }

bool SystemAudioCapture::start(int, int, bool exclusive) {
    if (isRunning()) return true;
    if (exclusive)
        VC_WARN("Phone-only output needs a virtual audio driver on Windows — "
                "capturing in shared mode (local playback stays on)");

    // Probe the capture device + mix rate synchronously so sampleRate() is
    // valid on return. Zero-speaker PCs have NO default render endpoint —
    // fall back to VB-CABLE's "CABLE Input" (the same driver the virtual mic
    // uses): the user sets it as the app's/system's output and the phone
    // becomes this computer's speaker.
    vcwin::ComPtr<IMMDevice> picked;
    {
        vcwin::ScopedCom com;
        if (!com.ok) return false;
        auto en = vcwin::makeEnumerator();
        if (!en) return false;
        vcwin::ComPtr<IMMDevice> dev;
        if (FAILED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) {
            vcwin::ComPtr<IMMDeviceCollection> devices;
            if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE,
                                                 &devices))) {
                UINT count = 0;
                devices->GetCount(&count);
                for (UINT i = 0; i < count && !dev; ++i) {
                    vcwin::ComPtr<IMMDevice> d;
                    if (FAILED(devices->Item(i, &d))) continue;
                    if (vcwin::deviceFriendlyName(d.Get()).contains(
                            QStringLiteral("CABLE Input"), Qt::CaseInsensitive))
                        dev = d;
                }
            }
            if (!dev) {
                VC_WARN("No playback device to capture — on a speakerless PC "
                        "install VB-CABLE (vb-audio.com/Cable) so apps have an "
                        "output and the phone can play it");
                return false;
            }
            VC_INFO("No default output — capturing VB-CABLE loopback instead");
        }
        vcwin::ComPtr<IAudioClient> probe;
        if (FAILED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                 reinterpret_cast<void **>(probe.GetAddressOf()))))
            return false;
        WAVEFORMATEX *wf = nullptr;
        if (FAILED(probe->GetMixFormat(&wf)) || !wf) return false;
        m_rate = int(wf->nSamplesPerSec);
        CoTaskMemFree(wf);
        picked = dev; // thread reuses the picked endpoint
    }
    m_channels = 2;

    auto *state = new WinLoopback();
    state->run = true;
    state->device = picked;
    m_win = state;

    state->thread = std::thread([this, state]() {
        vcwin::ScopedCom com;
        vcwin::ComPtr<IMMDevice> dev = state->device;
        vcwin::ComPtr<IAudioClient> client;
        vcwin::ComPtr<IAudioCaptureClient> capture;
        WAVEFORMATEX *wf = nullptr;

        bool ok = com.ok && dev &&
            SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                    reinterpret_cast<void **>(client.GetAddressOf()))) &&
            SUCCEEDED(client->GetMixFormat(&wf)) &&
            SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                         AUDCLNT_STREAMFLAGS_LOOPBACK,
                                         2000000 /* 200 ms */, 0, wf, nullptr)) &&
            SUCCEEDED(client->GetService(__uuidof(IAudioCaptureClient),
                                         reinterpret_cast<void **>(capture.GetAddressOf()))) &&
            SUCCEEDED(client->Start());
        if (!ok) {
            VC_WARN("WASAPI loopback init failed — system audio capture unavailable");
            if (wf) CoTaskMemFree(wf);
            return;
        }
        const vcwin::MixFormat fmt = vcwin::MixFormat::from(wf);
        CoTaskMemFree(wf);
        VC_INFO("System audio capture started (WASAPI loopback, {} Hz mix, {} ch)",
                fmt.rate, fmt.channels);

        const int chunkBytes = fmt.rate / 50 * 2 /*ch*/ * 2 /*s16*/;
        QByteArray pending;

        while (state->run) {
            Sleep(5);
            UINT32 packet = 0;
            while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
                BYTE *data = nullptr;
                UINT32 frames = 0;
                DWORD flags = 0;
                if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                    break;
                const int old = pending.size();
                pending.resize(old + int(frames) * 4);
                auto *out = reinterpret_cast<int16_t *>(pending.data() + old);
                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    std::memset(out, 0, size_t(frames) * 4);
                } else {
                    for (UINT32 i = 0; i < frames; ++i) {
                        const uint8_t *frame = data + size_t(i) * fmt.frameBytes;
                        const float l = vcwin::readSample(frame, fmt, 0);
                        const float r = fmt.channels > 1
                            ? vcwin::readSample(frame, fmt, 1) : l;
                        out[i * 2] = int16_t(qBound(-32768, int(l * 32767.0f), 32767));
                        out[i * 2 + 1] = int16_t(qBound(-32768, int(r * 32767.0f), 32767));
                    }
                }
                capture->ReleaseBuffer(frames);
                while (pending.size() >= chunkBytes) {
                    emit chunkReady(pending.left(chunkBytes));
                    pending.remove(0, chunkBytes);
                }
            }
        }
        client->Stop();
    });
    return true;
}

void SystemAudioCapture::stop() {
    auto *state = static_cast<WinLoopback *>(m_win);
    if (!state) return;
    state->run = false;
    if (state->thread.joinable()) state->thread.join();
    delete state;
    m_win = nullptr;
    VC_INFO("System audio capture stopped");
}

bool SystemAudioCapture::setupExclusiveRouting(int) { return false; }
void SystemAudioCapture::teardownExclusiveRouting() {}
void SystemAudioCapture::recoverStaleRouting(Settings *) {}

#else // other platforms — no audio backend

bool SystemAudioCapture::isRunning() const { return false; }
bool SystemAudioCapture::start(int, int, bool) {
    VC_WARN("System audio capture not supported on this platform yet");
    return false;
}
void SystemAudioCapture::stop() {}
bool SystemAudioCapture::setupExclusiveRouting(int) { return false; }
void SystemAudioCapture::teardownExclusiveRouting() {}
void SystemAudioCapture::recoverStaleRouting(Settings *) {}

#endif
