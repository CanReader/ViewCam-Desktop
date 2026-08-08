#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <atomic>
#include <memory>

class AnalyticsClient;
class EventQueue;
class QSettings;
class QTimer;

// Process-wide analytics facade — the only class the rest of the app calls.
//
// Every method is a no-op until init() has run and whenever the user has opted
// out, so callers may invoke it from any error path without guarding. Failures
// are swallowed by design: analytics must never be able to take down, slow
// down, or delay the app.
//
// Mirrors the shape of the mobile app's AppTelemetry facade so both codebases
// read the same way.
class Analytics : public QObject {
    Q_OBJECT

public:
    static Analytics &instance();

    // Reads settings, resolves the install ID, emits app_installed (first run
    // only) then app_started, and starts the heartbeat. Safe to call twice.
    void init();

    // Emits app_exited with session duration and flushes the queue to disk.
    void shutdown();

    // Thread-safe: calls from any thread are marshalled onto the Analytics
    // thread. The app runs StreamReceiver, FramePipeline and the audio sinks on
    // their own threads, and those are precisely the places errors surface —
    // so this has to be callable from them without the caller thinking about it.
    void capture(const QString &event, const QVariantMap &props = {});

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on);

    // Events buffered but not yet sent. Test seam.
    int pendingCount() const;

    // The persistent per-install UUID, generating and storing one on first
    // call. Bare 36-char form, no braces.
    static QString resolveInstallId(QSettings &s);

    // Version / OS / arch / channel. Contains nothing identifying a machine or
    // a person — enforced by test, not merely by intent.
    static QVariantMap buildSuperProperties();

private:
    explicit Analytics(QObject *parent = nullptr);
    ~Analytics() override;

    // Runs on the Analytics thread only. capture() marshals here when called
    // from elsewhere. Takes the timestamp as a parameter so it reflects when
    // the event HAPPENED, not when the queued call was delivered.
    void enqueue(const QString &event, const QVariantMap &props, qint64 tsMs);

    void flush();

    // Read from worker threads, written from the main thread. Atomic for the
    // same reason mobile's AppTelemetry marks isEnabled @Volatile: a stale
    // read there silently dropped reports from background error paths.
    std::atomic<bool>           m_enabled{false};
    std::atomic<bool>           m_initialized{false};
    bool                        m_debugOnly   = false;
    qint64                      m_startedMs   = 0;
    QString                     m_installId;
    std::unique_ptr<EventQueue> m_queue;
    AnalyticsClient            *m_client     = nullptr;
    QTimer                     *m_heartbeat  = nullptr;
    QTimer                     *m_flushTimer = nullptr;
};
