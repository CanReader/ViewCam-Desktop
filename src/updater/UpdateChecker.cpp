#include "updater/UpdateChecker.h"
#include "ViewCamConfig.h"
#include "core/Logger.h"
#include "updater/InstallLayout.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <QVersionNumber>

#include <openssl/evp.h>

namespace {

constexpr int CHECK_TIMEOUT_MS    = 8000;
constexpr int DOWNLOAD_TIMEOUT_MS = 0;  // no transfer timeout while downloading

// QSettings key for the user-selected update channel (overrides the
// compiled-in VIEWCAM_UPDATE_CHANNEL when set).
constexpr char CHANNEL_KEY[] = "updates/channel";

QByteArray hexToBytes(const QByteArray &hex) {
    return QByteArray::fromHex(hex);
}

}  // namespace

UpdateChecker *UpdateChecker::s_instance = nullptr;

UpdateChecker *UpdateChecker::create(QQmlEngine *, QJSEngine *) {
    return instance();
}

UpdateChecker *UpdateChecker::instance() {
    if (!s_instance)
        s_instance = new UpdateChecker();
    return s_instance;
}

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_channel = settingsChannel();
}

// ── Channel / manifest URL ─────────────────────────────────────────────────

QString UpdateChecker::settingsChannel() {
    // Persisted Settings override → compiled-in default. Only "stable"/"beta"
    // are accepted; anything else falls back to the compiled-in channel.
    QSettings s;
    const QString saved =
        s.value(QLatin1String(CHANNEL_KEY)).toString().trimmed().toLower();
    if (saved == QLatin1String("stable") || saved == QLatin1String("beta"))
        return saved;
    return QString::fromLatin1(VIEWCAM_UPDATE_CHANNEL);
}

void UpdateChecker::saveChannel(const QString &c) {
    QSettings s;
    s.setValue(QLatin1String(CHANNEL_KEY), c);
}

QString UpdateChecker::manifestUrl() const {
    return QStringLiteral("%1/%2/manifest.json")
        .arg(QLatin1String(VIEWCAM_UPDATE_MANIFEST_BASE), m_channel);
}

void UpdateChecker::setChannel(const QString &c) {
    const QString want = c.trimmed().toLower();
    if (want != QLatin1String("stable") && want != QLatin1String("beta"))
        return;
    if (m_channel == want)
        return;
    m_channel = want;
    saveChannel(want);
    emit channelChanged();
    VC_INFO("Update channel changed to '{}' — re-checking", want.toStdString());
    // Clear any prior result and re-check against the new channel. If a check
    // is mid-flight start() no-ops; the user can re-check manually.
    start();
}

QString UpdateChecker::currentVersion() const {
    return QString::fromLatin1(VIEWCAM_VERSION_STRING);
}

bool UpdateChecker::installable() const {
    const QString root = vcam::installRoot();
    const QString exe  = QCoreApplication::applicationFilePath();
    return vcam::isSelfUpdatable(root, exe, nullptr);
}

// ── Shutdown hook: apply on quit when "Install on quit" was chosen ──────────

void UpdateChecker::applyPendingOnQuit() {
    if (!m_applyOnQuit)
        return;
    if (m_state != State::ReadyToApply) {
        VC_INFO("applyPendingOnQuit: nothing verified to apply (state {})",
                static_cast<int>(m_state));
        return;
    }
    VC_INFO("applyPendingOnQuit: applying staged update {} on shutdown",
            m_latestVersion.toStdString());
    // applyStaged() spawns the helper and calls QCoreApplication::quit(); during
    // aboutToQuit that's a harmless no-op — the helper is already detached.
    applyStaged();
}

// ── Step 1: check ──────────────────────────────────────────────────────────

void UpdateChecker::start() {
    if (m_state == State::Checking || m_state == State::Downloading ||
        m_state == State::Verifying || m_state == State::Applying)
        return;

    setState(State::Checking);
    setStatusText(tr("Checking for updates…"));
    setProgress(0);

    const QString url = manifestUrl();
    VC_INFO("Update check: GET {}", url.toStdString());

    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(CHECK_TIMEOUT_MS);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onManifestReply(reply); });
}

void UpdateChecker::onManifestReply(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        fail("manifest fetch failed: " + reply->errorString(),
             tr("Could not check for updates — network error"));
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        fail("manifest is not a JSON object",
             tr("Could not check for updates — bad manifest"));
        return;
    }
    const QJsonObject obj = doc.object();

    const QString remoteStr  = obj.value(QStringLiteral("version")).toString();
    const QString minVerStr  = obj.value(QStringLiteral("min_version")).toString();
    const QString notesUrl   = obj.value(QStringLiteral("notes_url")).toString();

    if (remoteStr.isEmpty()) {
        fail("manifest missing version field",
             tr("Could not check for updates — bad manifest"));
        return;
    }

    qsizetype suffixIdx = 0;
    const QVersionNumber remote = QVersionNumber::fromString(remoteStr, &suffixIdx);
    const QVersionNumber local  =
        QVersionNumber::fromString(QLatin1String(VIEWCAM_VERSION_STRING), &suffixIdx);

    // Honor min_version: if our current version is older than the manifest's
    // declared minimum, we still treat the published version as the target
    // (the upgrade is required) — we simply log it. The comparison that gates
    // "is there a newer build" is remote vs local below.
    if (!minVerStr.isEmpty()) {
        const QVersionNumber minVer = QVersionNumber::fromString(minVerStr, &suffixIdx);
        if (local < minVer) {
            VC_WARN("Current version {} is below manifest min_version {} — update required",
                    VIEWCAM_VERSION_STRING, minVerStr.toStdString());
        }
    }

    if (remote <= local) {
        VC_INFO("Up to date (local={}, remote={})", VIEWCAM_VERSION_STRING,
                remoteStr.toStdString());
        setStatusText(tr("ViewCam is up to date"));
        setState(State::UpToDate);
        return;
    }

    // Select the platform entry.
    const QString key = osArchKey();
    if (key.isEmpty()) {
        fail("unsupported OS/arch for self-update",
             tr("Update %1 available — visit viewcam.tech").arg(remoteStr));
        return;
    }
    const QJsonObject platforms =
        obj.value(QStringLiteral("platforms")).toObject();
    const QJsonObject entry = platforms.value(key).toObject();
    if (entry.isEmpty()) {
        VC_WARN("No platform entry '{}' in manifest", key.toStdString());
        fail("no platform entry for " + key,
             tr("Update %1 available — visit viewcam.tech").arg(remoteStr));
        return;
    }

    const QString    url   = entry.value(QStringLiteral("url")).toString();
    const QString    sha   = entry.value(QStringLiteral("sha256")).toString().toLower();
    const qint64     size  = entry.value(QStringLiteral("size")).toVariant().toLongLong();
    const QString    sigB64 = entry.value(QStringLiteral("signature")).toString();
    const QByteArray sig   = QByteArray::fromBase64(sigB64.toLatin1());

    if (url.isEmpty() || sha.isEmpty() || size <= 0 || sig.size() != 64) {
        fail(QStringLiteral("incomplete platform entry (url=%1 sha=%2 size=%3 sigLen=%4)")
                 .arg(url.isEmpty() ? "∅" : "set",
                      sha.isEmpty() ? "∅" : "set")
                 .arg(size)
                 .arg(sig.size()),
             tr("Update %1 available — visit viewcam.tech").arg(remoteStr));
        return;
    }

    m_archiveUrl         = url;
    m_expectedSha256Hex  = sha;
    m_expectedSize       = size;
    m_expectedSignature  = sig;

    setLatestVersion(remoteStr);
    setNotesUrl(notesUrl);
    VC_INFO("Update available: {} -> {} ({}, {} bytes)", VIEWCAM_VERSION_STRING,
            remoteStr.toStdString(), key.toStdString(), size);

    setStatusText(tr("Update %1 is available").arg(remoteStr));
    setState(State::UpdateAvailable);

    // Do NOT auto-download by default; the UI calls download(). The
    // m_autoDownload path is here for a future "automatic" setting.
    if (m_autoDownload)
        download();
}

// ── Step 2: download to staging ────────────────────────────────────────────

void UpdateChecker::download() {
    if (m_state != State::UpdateAvailable) {
        VC_WARN("download() called in state {} — ignoring",
                static_cast<int>(m_state));
        return;
    }
    if (m_archiveUrl.isEmpty()) {
        fail("download() with no archive URL", tr("Download failed"));
        return;
    }

    const QString dir = stagingDir();
    if (dir.isEmpty()) {
        fail("could not create staging dir",
             tr("Install failed — cannot write to disk"));
        return;
    }

    // Final staged path is …/staging/<version>; we stream to <path>.part first.
    m_stagedPath = QDir(dir).absoluteFilePath(m_latestVersion);
    const QString partPath = m_stagedPath + QStringLiteral(".part");
    QFile::remove(partPath);  // drop any prior partial

    auto *part = new QFile(partPath);
    if (!part->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete part;
        fail("cannot open staging .part for write: " + partPath,
             tr("Install failed — cannot write to disk"));
        return;
    }

    setState(State::Downloading);
    setStatusText(tr("Downloading %1…").arg(m_latestVersion));
    setProgress(0);
    VC_INFO("Downloading {} -> {}", m_archiveUrl.toStdString(),
            partPath.toStdString());

    QNetworkRequest req((QUrl(m_archiveUrl)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(DOWNLOAD_TIMEOUT_MS);

    QNetworkReply *reply = m_nam->get(req);

    // Stream to disk as data arrives (don't buffer the whole archive in RAM).
    connect(reply, &QNetworkReply::readyRead, this, [reply, part]() {
        part->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0)
                    setProgress(static_cast<int>(received * 100 / total));
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, part, partPath]() {
                part->write(reply->readAll());
                part->flush();
                part->close();
                reply->deleteLater();

                if (reply->error() != QNetworkReply::NoError) {
                    QFile::remove(partPath);
                    delete part;
                    fail("download failed: " + reply->errorString(),
                         tr("Download failed — visit viewcam.tech"));
                    return;
                }

                // .part → final staged name.
                QFile::remove(m_stagedPath);
                if (!QFile::rename(partPath, m_stagedPath)) {
                    QFile::remove(partPath);
                    delete part;
                    fail("could not finalize staged file: " + m_stagedPath,
                         tr("Install failed — cannot write to disk"));
                    return;
                }
                delete part;
                onDownloadFinished(reply);
            });
}

void UpdateChecker::onDownloadFinished(QNetworkReply * /*reply*/) {
    VC_INFO("Download complete: {}", m_stagedPath.toStdString());
    verifyStaged();
}

// ── Step 3: verify (size → SHA-256 → Ed25519) ──────────────────────────────

void UpdateChecker::verifyStaged() {
    setState(State::Verifying);
    setStatusText(tr("Verifying download…"));

    QFileInfo fi(m_stagedPath);

    // 1) Size must match the manifest exactly.
    if (fi.size() != m_expectedSize) {
        fail(QStringLiteral("size mismatch: got %1, expected %2")
                 .arg(fi.size())
                 .arg(m_expectedSize),
             tr("Update verification failed — size mismatch"),
             /*removeStagedFile=*/true);
        return;
    }

    // 2) SHA-256 integrity (streamed).
    bool hashOk = false;
    const QByteArray digest = sha256File(m_stagedPath, &hashOk);
    if (!hashOk) {
        fail("could not read staged file for hashing",
             tr("Update verification failed — cannot read download"),
             /*removeStagedFile=*/true);
        return;
    }
    const QString gotSha = QString::fromLatin1(digest.toHex());
    if (gotSha != m_expectedSha256Hex) {
        fail("SHA-256 mismatch: got " + gotSha + " expected " + m_expectedSha256Hex,
             tr("Update verification failed — checksum mismatch"),
             /*removeStagedFile=*/true);
        return;
    }

    // 3) Ed25519 authenticity over the ARCHIVE BYTES with the embedded pubkey.
    // OpenSSL's one-shot Ed25519 verify needs the whole message in memory; the
    // size is already gated against the manifest above, so this is bounded.
    QFile f(m_stagedPath);
    if (!f.open(QIODevice::ReadOnly)) {
        fail("could not reopen staged file for signature check",
             tr("Update verification failed — cannot read download"),
             /*removeStagedFile=*/true);
        return;
    }
    const QByteArray archive = f.readAll();
    f.close();

    const QByteArray pubKey = hexToBytes(QByteArray(VIEWCAM_UPDATE_PUBKEY));
    if (pubKey.size() != 32) {
        fail("embedded pubkey is not 32 bytes — build misconfigured",
             tr("Update verification failed — bad signing key"),
             /*removeStagedFile=*/true);
        return;
    }

    if (!verifyEd25519(archive, m_expectedSignature, pubKey)) {
        fail("Ed25519 signature verification FAILED",
             tr("Update verification failed — invalid signature"),
             /*removeStagedFile=*/true);
        return;
    }

    VC_INFO("Verified {} ({} bytes): size+SHA-256+Ed25519 all OK",
            m_latestVersion.toStdString(), m_expectedSize);
    setStatusText(tr("Update %1 verified and ready").arg(m_latestVersion));
    setState(State::ReadyToApply);

    // Phase 1 stops here. applyStaged() is the seam for Phase 2.
}

// ── Step 4 seam (Phase 2): extract + atomic swap via vc-updater ────────────

void UpdateChecker::applyStaged() {
    if (m_state != State::ReadyToApply) {
        VC_WARN("applyStaged() called in state {} — ignoring",
                static_cast<int>(m_state));
        return;
    }
    if (m_stagedPath.isEmpty() || !QFileInfo::exists(m_stagedPath)) {
        fail("applyStaged() with no verified staged archive",
             tr("Install failed — nothing staged"));
        return;
    }

    const QString root = vcam::installRoot();
    const QString runningExe = QCoreApplication::applicationFilePath();

    // §1: never swap a system/packaged or read-only install — notify instead.
    QString why;
    if (!vcam::isSelfUpdatable(root, runningExe, &why)) {
        VC_WARN("Self-update not possible ({}); deferring to installer", why.toStdString());
        setStatusText(tr("Update %1 is available — install it from viewcam.tech")
                          .arg(m_latestVersion));
        setState(State::Error);
        return;
    }

    const QString newVersionDir = vcam::versionDir(root, m_latestVersion);

    // Already extracted on a previous attempt? Skip re-extraction (idempotent).
    const QString appExe = QDir(newVersionDir).absoluteFilePath(vcam::appExeName());
    if (!QFileInfo::exists(appExe)) {
        // Extract to a scratch dir under staging/, then atomically rename into
        // versions/<v>. Never write directly into versions/ in case we crash
        // mid-extract and leave a partial, "valid-looking" version dir.
        const QString scratch =
            QDir(root).absoluteFilePath(QStringLiteral("staging/_extract-") + m_latestVersion);
        QDir(scratch).removeRecursively();

        VC_INFO("Extracting {} -> {}", m_stagedPath.toStdString(), scratch.toStdString());
        QString exErr;
        if (!vcam::extractZip(m_stagedPath, scratch, &exErr)) {
            QDir(scratch).removeRecursively();
            fail("extraction failed: " + exErr,
                 tr("Install failed — could not unpack update"));
            return;
        }
        if (!QFileInfo::exists(QDir(scratch).absoluteFilePath(vcam::appExeName()))) {
            QDir(scratch).removeRecursively();
            fail("extracted archive has no " + vcam::appExeName(),
                 tr("Install failed — update package is malformed"));
            return;
        }

        QDir().mkpath(vcam::versionsDir(root));
        QDir(newVersionDir).removeRecursively();  // drop any stale partial
        if (!QDir().rename(scratch, newVersionDir)) {
            QDir(scratch).removeRecursively();
            fail("could not move extracted version into place: " + newVersionDir,
                 tr("Install failed — cannot write to disk"));
            return;
        }
    } else {
        VC_INFO("Version {} already extracted — reusing", m_latestVersion.toStdString());
    }

    // Old version dir = whatever `current` points at right now (may be empty on
    // a first-ever install; the helper tolerates that).
    const QString oldVersionDir = vcam::resolveCurrent(root);

    // Helper lives at the install root, NOT inside any version dir, so it can
    // safely swap the dir it would otherwise be running from.
    const QString helper = QDir(root).absoluteFilePath(vcam::helperExeName());
    if (!QFileInfo::exists(helper)) {
        fail("vc-updater helper missing at " + helper,
             tr("Install failed — updater helper not found"));
        return;
    }

    const QString relaunchPath = QDir(newVersionDir).absoluteFilePath(vcam::appExeName());
    const QStringList args = {
        oldVersionDir,                         // may be ""
        newVersionDir,
        root,
        QString::number(QCoreApplication::applicationPid()),
        relaunchPath,
    };

    VC_INFO("Launching vc-updater: {} [old={} new={} root={} pid={}]",
            helper.toStdString(), oldVersionDir.toStdString(),
            newVersionDir.toStdString(), root.toStdString(),
            QCoreApplication::applicationPid());

    if (!QProcess::startDetached(helper, args, root)) {
        fail("could not spawn vc-updater helper",
             tr("Install failed — could not start updater"));
        return;
    }

    setStatusText(tr("Installing update %1…").arg(m_latestVersion));
    setState(State::Applying);

    // Hand off: quit so the helper can repoint `current` and relaunch us. We
    // never touch in-use files ourselves.
    QCoreApplication::quit();
}

// ── First-run sentinel (post-update success signal to the helper) ──────────

void UpdateChecker::clearPendingVerifyIfJustUpdated(const QStringList &args) {
    int idx = args.indexOf(QStringLiteral("--just-updated"));
    if (idx < 0)
        return;
    const QString ver = (idx + 1 < args.size()) ? args.at(idx + 1) : QString{};
    VC_INFO("Started with --just-updated {} — clearing post-update sentinel",
            ver.toStdString());

    const QString root = vcam::installRoot();
    const QString cur  = vcam::currentLink(root);
    // Clear the sentinel + attempt counter under current/. Use the resolved
    // target so we delete the real file even if `current` is a symlink.
    const QString dir  = vcam::resolveCurrent(root);
    for (const QString &base : {cur, dir}) {
        if (base.isEmpty()) continue;
        QFile::remove(QDir(base).absoluteFilePath(vcam::pendingVerifyName()));
        QFile::remove(QDir(base).absoluteFilePath(vcam::attemptsName()));
    }
}

// ── Crypto / platform helpers ──────────────────────────────────────────────

QByteArray UpdateChecker::sha256File(const QString &path, bool *ok) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) {  // streams the file in chunks
        if (ok) *ok = false;
        return {};
    }
    if (ok) *ok = true;
    return hash.result();
}

bool UpdateChecker::verifyEd25519(const QByteArray &message,
                                  const QByteArray &signature,
                                  const QByteArray &rawPubKey) {
    if (rawPubKey.size() != 32 || signature.size() != 64)
        return false;

    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(
        EVP_PKEY_ED25519, nullptr,
        reinterpret_cast<const unsigned char *>(rawPubKey.constData()),
        static_cast<size_t>(rawPubKey.size()));
    if (!pkey)
        return false;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        EVP_PKEY_free(pkey);
        return false;
    }

    bool ok = false;
    // Ed25519 is a one-shot scheme: digest type must be nullptr and the whole
    // message is passed to EVP_DigestVerify (not a streaming Update/Final).
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        const int rc = EVP_DigestVerify(
            ctx,
            reinterpret_cast<const unsigned char *>(signature.constData()),
            static_cast<size_t>(signature.size()),
            reinterpret_cast<const unsigned char *>(message.constData()),
            static_cast<size_t>(message.size()));
        ok = (rc == 1);
    }

    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

QString UpdateChecker::osArchKey() {
    // Phase 1 supports x86_64 only; the manifest keys mirror this.
    const QString arch = QSysInfo::currentCpuArchitecture();  // "x86_64", "arm64", …
    if (arch != QLatin1String("x86_64"))
        return {};
#if defined(Q_OS_LINUX)
    return QStringLiteral("linux-x86_64");
#elif defined(Q_OS_WIN)
    return QStringLiteral("windows-x86_64");
#else
    return {};
#endif
}

QString UpdateChecker::stagingDir() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        return {};
    const QString dir = QDir(base).absoluteFilePath(QStringLiteral("staging"));
    if (!QDir().mkpath(dir))
        return {};
    return dir;
}

// ── State plumbing ─────────────────────────────────────────────────────────

void UpdateChecker::fail(const QString &logMsg, const QString &userMsg,
                         bool removeStagedFile) {
    VC_ERROR("Update: {}", logMsg.toStdString());
    if (removeStagedFile && !m_stagedPath.isEmpty()) {
        if (QFile::remove(m_stagedPath))
            VC_INFO("Removed rejected staged file: {}", m_stagedPath.toStdString());
    }
    setStatusText(userMsg);
    setState(State::Error);
}

void UpdateChecker::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void UpdateChecker::setStatusText(const QString &t) {
    if (m_statusText == t) return;
    m_statusText = t;
    emit statusTextChanged();
}

void UpdateChecker::setProgress(int p) {
    if (m_progress == p) return;
    m_progress = p;
    emit progressChanged();
}

void UpdateChecker::setLatestVersion(const QString &v) {
    if (m_latestVersion == v) return;
    m_latestVersion = v;
    emit latestVersionChanged();
}

void UpdateChecker::setNotesUrl(const QString &u) {
    if (m_notesUrl == u) return;
    m_notesUrl = u;
    emit notesUrlChanged();
}
