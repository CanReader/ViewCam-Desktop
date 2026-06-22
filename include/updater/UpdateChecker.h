#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QNetworkReply;
class QFile;

// ──────────────────────────────────────────────────────────────────────────
// UpdateChecker — Phase 1: signed full-package update CHECK + DOWNLOAD +
// VERIFY (no in-place swap, no extraction).
//
// Flow: GET <base>/<channel>/manifest.json → parse → version/min_version
// compare → emit UpdateAvailable (no auto-download). UI calls download() →
// stream archive to staging/<version>.part → Verifying (size, SHA-256, then
// Ed25519 over the archive bytes with the embedded pubkey). Any mismatch
// deletes the file and ends in Error — we NEVER proceed on a failed check.
// On success → ReadyToApply. Phase 2 (vc-updater helper, atomic swap,
// extraction) launches from the ReadyToApply seam in applyStaged().
// ──────────────────────────────────────────────────────────────────────────
class UpdateChecker : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class State {
        Idle,             // nothing in progress, no check run yet
        Checking,         // fetching/parsing manifest
        UpdateAvailable,  // newer version found, awaiting download()
        Downloading,      // streaming archive to staging
        Verifying,        // size + SHA-256 + Ed25519
        ReadyToApply,     // verified; Phase 2 swap can run
        Applying,         // Phase 2 swap in progress (helper handoff)
        UpToDate,         // already on latest
        Error             // any failure; statusText has detail
    };
    Q_ENUM(State)

    Q_PROPERTY(State   state         READ state         NOTIFY stateChanged)
    Q_PROPERTY(int     progress      READ progress      NOTIFY progressChanged)
    Q_PROPERTY(QString statusText    READ statusText    NOTIFY statusTextChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY latestVersionChanged)
    Q_PROPERTY(QString notesUrl      READ notesUrl      NOTIFY notesUrlChanged)
    // Active update channel ("stable" / "beta"). Defaults to the compiled-in
    // VIEWCAM_UPDATE_CHANNEL but is overridable from Settings at runtime; the
    // manifest URL is rebuilt from it on every check. Writing a new value
    // re-checks against the new channel.
    Q_PROPERTY(QString channel       READ channel       WRITE setChannel NOTIFY channelChanged)
    // True when this install can self-update (running under the per-user
    // writable root). False for system/packaged installs → UI shows
    // "get it from viewcam.tech" instead of an in-app install button.
    Q_PROPERTY(bool    installable   READ installable   CONSTANT)
    // The version string this build currently runs as (for the Settings card).
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)

    static UpdateChecker *create(QQmlEngine *, QJSEngine *);
    static UpdateChecker *instance();

    State   state()         const { return m_state; }
    int     progress()      const { return m_progress; }
    QString statusText()    const { return m_statusText; }
    QString latestVersion() const { return m_latestVersion; }
    QString notesUrl()      const { return m_notesUrl; }
    QString channel()       const { return m_channel; }
    bool    installable()   const;
    QString currentVersion() const;

    void setChannel(const QString &c);

    // Kick off a manifest check (background-safe; no-op while busy).
    void start();

public slots:
    // QML-facing. checkNow() is an alias for start() (manual "Check now").
    void checkNow() { start(); }
    // Download the archive for the currently-available update, then verify.
    // Only valid in UpdateAvailable; no-op otherwise.
    void download();

    // Apply the verified staged update (Phase 2): extract into versions/<v>,
    // spawn the vc-updater helper, and quit. Only valid in ReadyToApply.
    void apply() { applyStaged(); }

    // "Install on quit": mark the verified update to be applied during a normal
    // app shutdown. main() connects aboutToQuit → applyPendingOnQuit(). Until
    // then the app keeps running normally on the current version.
    void installOnQuit() { m_applyOnQuit = true; }
    void cancelInstallOnQuit() { m_applyOnQuit = false; }
    bool isApplyPendingOnQuit() const { return m_applyOnQuit; }

    // Shutdown hook: if installOnQuit() was chosen and the update verified
    // (ReadyToApply), run apply() so the helper swaps + relaunches on exit.
    // No-op otherwise. Safe to call unconditionally from aboutToQuit.
    void applyPendingOnQuit();

    // Startup hook: if launched with --just-updated, clear the .pending-verify
    // sentinel so the helper/launcher knows this version booted successfully.
    // Safe to call unconditionally; no-op when the flag/sentinel is absent.
    static void clearPendingVerifyIfJustUpdated(const QStringList &args);

    // When true, an UpdateAvailable transition auto-calls download(). Off by
    // default in Phase 1 — the UI drives download(). Wired here for later.
    void setAutoDownload(bool on) { m_autoDownload = on; }

signals:
    void stateChanged();
    void progressChanged();
    void statusTextChanged();
    void latestVersionChanged();
    void notesUrlChanged();
    void channelChanged();

private:
    explicit UpdateChecker(QObject *parent = nullptr);

    void setState(State s);
    void setStatusText(const QString &t);
    void setProgress(int p);
    void setLatestVersion(const QString &v);
    void setNotesUrl(const QString &u);

    // Build <base>/<channel>/manifest.json from the runtime channel.
    QString manifestUrl() const;
    // Channel resolution: persisted Settings override → compiled-in default.
    static QString settingsChannel();
    static void    saveChannel(const QString &c);

    // ── Network with bounded exponential backoff ───────────────────────────
    // Both the manifest fetch and the archive download retry on TRANSIENT
    // network errors only (timeouts, connection drops). HTTP 4xx and any
    // verify failure are permanent → no retry.
    void startManifestFetch();    // (re)issue the manifest GET
    void onManifestReply(QNetworkReply *reply);
    void startDownload();         // (re)issue the archive GET (resume-aware)
    void onDownloadFinished(QNetworkReply *reply);
    void verifyStaged();          // size + SHA-256 + Ed25519
    void applyStaged();           // platform-split apply (Linux swap / Win installer)
#if defined(Q_OS_WIN)
    // Windows apply: launch the verified Setup.exe (it self-elevates) and quit.
    void applyWindowsInstaller();
#endif

    // True if `err` is a transient network failure worth retrying.
    static bool isTransient(QNetworkReply *reply);
    // Backoff delay for attempt n (0-based): 1s, 2s, 4s … capped.
    static int  backoffMs(int attempt);

    // Fail helper: log, set Error + user message, optionally remove the staged
    // file. Returns nothing; callers should `return` after invoking.
    void fail(const QString &logMsg, const QString &userMsg,
              bool removeStagedFile = false);

    // Static, pure helpers (no I/O) — also unit-testable in isolation.
    static QByteArray sha256File(const QString &path, bool *ok);
    static bool verifyEd25519(const QByteArray &message,
                              const QByteArray &signature,
                              const QByteArray &rawPubKey);
    static QString osArchKey();        // "linux-x86_64" / "windows-x86_64" / {}
    static QString stagingDir();       // …/ViewCam/staging  (created on demand)

    // Staged rollout: a STICKY per-install random id (persisted via QSettings)
    // hashed to a stable fraction in [0,1). An update is only surfaced when that
    // fraction < manifest "rollout". The same device never flip-flops.
    static QString installId();        // persisted; generated once on first read
    static double  rolloutFraction();  // hash(installId) → [0,1)

    // The basename the staged artifact is written as. Linux: the version string
    // (a .zip). Windows: "<version>.exe" so ShellExecute treats it as runnable.
    QString stagedArtifactName() const;

    State   m_state    = State::Idle;
    int     m_progress = 0;
    QString m_statusText;
    QString m_latestVersion;
    QString m_notesUrl;
    QString m_channel;        // resolved at construction; settable from QML

    bool    m_autoDownload = false;
    bool    m_applyOnQuit   = false;  // "Install on quit" was chosen

    // Manifest entry selected for this platform (set on UpdateAvailable).
    QString    m_archiveUrl;
    QString    m_expectedSha256Hex;   // lowercase hex
    QByteArray m_expectedSignature;   // raw bytes (decoded from base64)
    qint64     m_expectedSize = -1;

    QString    m_stagedPath;          // …/staging/<artifact> (final, post-rename)

    // Bounded-retry bookkeeping (reset at the start of each check/download).
    int m_manifestAttempt = 0;        // 0-based attempt counter for the manifest
    int m_downloadAttempt = 0;        // 0-based attempt counter for the download

    QNetworkAccessManager *m_nam = nullptr;
    static UpdateChecker  *s_instance;
};
