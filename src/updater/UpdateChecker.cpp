#include "updater/UpdateChecker.h"
#include "ViewCamConfig.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QVersionNumber>

static constexpr const char *MANIFEST_URL =
    "https://gist.githubusercontent.com/CanReader/"
    "b2043d728389a21a4668a251cc15960d/raw/viewcam-version.json";

static constexpr int CHECK_TIMEOUT_MS = 5000;

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
}

void UpdateChecker::start() {
    if (m_state == State::Checking || m_state == State::Downloading ||
        m_state == State::Applying)
        return;
    setState(State::Checking);
    setStatusText(tr("Checking for updates…"));

    QNetworkRequest req((QUrl(QLatin1String(MANIFEST_URL))));
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
        const QString err = reply->errorString();
        VC_WARN("Update check failed: {}", err.toStdString());
        finish("network error", err);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (!doc.isObject()) {
        finish("invalid manifest", tr("Could not check for updates"));
        return;
    }

    const QJsonObject obj = doc.object();
    const QString remoteStr = obj["version"].toString();

    qsizetype suffixIdx = 0;
    const QVersionNumber remote = QVersionNumber::fromString(remoteStr, &suffixIdx);
    const QVersionNumber local  = QVersionNumber::fromString(
        QLatin1String(VIEWCAM_VERSION_STRING), &suffixIdx);

    if (remote <= local) {
        VC_INFO("Up to date (local={}, remote={})",
                VIEWCAM_VERSION_STRING, remoteStr.toStdString());
        finish({}, tr("ViewCam is up to date"));
        return;
    }

    m_latestVersion = remoteStr;
    emit latestVersionChanged();
    VC_INFO("Update available: {} -> {}", VIEWCAM_VERSION_STRING, remoteStr.toStdString());

#if defined(Q_OS_LINUX)
    const QString url = obj["download_url_linux"].toString();
#elif defined(Q_OS_WIN)
    const QString url = obj["download_url_windows"].toString();
#else
    const QString url;
#endif

    if (url.isEmpty()) {
        VC_WARN("Update available but no download URL for this platform, skipping");
        finish("no download URL",
               tr("Update %1 available — visit viewcam.tech").arg(m_latestVersion));
        return;
    }

    startDownload(url);
}

void UpdateChecker::startDownload(const QString &url) {
    setState(State::Downloading);
    setStatusText(tr("Downloading %1…").arg(m_latestVersion));
    setProgress(0);

    QNetworkRequest req((QUrl(url)));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0)
                    setProgress(static_cast<int>(received * 100 / total));
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { onDownloadFinished(reply); });
}

void UpdateChecker::onDownloadFinished(QNetworkReply *reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        VC_WARN("Download failed: {}", reply->errorString().toStdString());
        finish("download failed", tr("Download failed — visit viewcam.tech"));
        return;
    }

    m_downloadedBytes = reply->readAll();
    applyUpdate();
}

void UpdateChecker::applyUpdate() {
    setState(State::Applying);
    setStatusText(tr("Installing update…"));

    const QString exePath = QCoreApplication::applicationFilePath();

#if defined(Q_OS_LINUX)
    const QString tmpPath = exePath + QStringLiteral(".new");

    QFile f(tmpPath);
    if (!f.open(QIODevice::WriteOnly)) {
        VC_ERROR("Cannot write update binary to {}", tmpPath.toStdString());
        finish("cannot write to disk", tr("Install failed — cannot write to disk"));
        return;
    }
    qint64 written = f.write(m_downloadedBytes);
    f.close();
    if (written != static_cast<qint64>(m_downloadedBytes.size())) {
        VC_ERROR("Update write incomplete ({} of {} bytes), disk full?",
                 written, m_downloadedBytes.size());
        QFile::remove(tmpPath);
        finish("write incomplete", tr("Install failed — disk full?"));
        return;
    }

    // Make executable
    f.setPermissions(
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup |
        QFileDevice::ReadOther  | QFileDevice::ExeOther);

    // Atomic rename — the running process keeps its inode, the path is free to move
    if (!QFile::rename(tmpPath, exePath)) {
        VC_ERROR("Failed to replace binary at {}", exePath.toStdString());
        QFile::remove(tmpPath);
        finish("failed to replace binary", tr("Install failed — permission error"));
        return;
    }

    VC_INFO("Update applied, relaunching");
    QProcess::startDetached(exePath, {});
    QCoreApplication::quit();

#elif defined(Q_OS_WIN)
    const QString tmpExe  = QDir::temp().absoluteFilePath("viewcam-update.exe");
    const QString batPath = QDir::temp().absoluteFilePath("viewcam-apply.bat");

    QFile f(tmpExe);
    if (!f.open(QIODevice::WriteOnly)) {
        finish("Cannot write update to temp folder.");
        return;
    }
    qint64 written = f.write(m_downloadedBytes);
    f.close();
    if (written != static_cast<qint64>(m_downloadedBytes.size())) {
        finish("Cannot write complete update to temp folder — disk full?");
        return;
    }

    // bat: wait for this process to exit, swap, relaunch, self-delete
    const QString bat = QStringLiteral(
        "@echo off\r\n"
        ":wait\r\n"
        "tasklist /FI \"IMAGENAME eq %3\" 2>nul | find /I \"%3\" >nul && "
        "(ping 127.0.0.1 -n 2 >nul & goto wait)\r\n"
        "copy /Y \"%1\" \"%2\"\r\n"
        "start \"\" \"%2\"\r\n"
        "del \"%0\"\r\n"
    ).arg(tmpExe, exePath, QFileInfo(exePath).fileName());

    QFile batFile(batPath);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        finish("Cannot write update script.");
        return;
    }
    batFile.write(bat.toLocal8Bit());
    batFile.close();

    VC_INFO("Launching update bat, quitting");
    QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), batPath});
    QCoreApplication::quit();

#else
    VC_WARN("Auto-update not supported on this platform");
    finish();
#endif
}

void UpdateChecker::finish(const QString &logMsg, const QString &userMsg) {
    if (!logMsg.isEmpty())
        VC_WARN("Update: {}", logMsg.toStdString());
    setState(logMsg.isEmpty() ? State::UpToDate : State::Error);
    setStatusText(userMsg);
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
