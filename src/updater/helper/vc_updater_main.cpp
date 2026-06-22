// ──────────────────────────────────────────────────────────────────────────
// vc-updater — standalone post-download swap/rollback helper (QtCore only).
//
// Lives at the install ROOT (never inside a version dir) so it can swap the
// dir the app runs from. Spawned detached by UpdateChecker::applyStaged() once
// a new version is extracted into versions/<new>.
//
// CLI contract (positional):
//   vc-updater <oldVersionDir> <newVersionDir> <installRoot> <parentPid> <relaunchPath>
//
//   oldVersionDir  current target before swap; may be "" on first install
//   newVersionDir  versions/<new> (already extracted + verified by the app)
//   installRoot    <root>; current/, versions/, update.log live here
//   parentPid      pid of the quitting app; we wait (bounded) for it to exit
//   relaunchPath   absolute path to current/ViewCam after activation
//
// Behavior (brief §5):
//   1. wait for parentPid to exit (bounded)
//   2. activate: repoint <root>/current -> versions/<new> atomically
//   3. write current/.pending-verify, relaunch newVersion with
//      --just-updated <new>
//   4. on a failed/looping launch (sentinel still set after N attempts):
//      roll back current -> old version, log it
//   5. prune versions beyond the newest 2
// Idempotent + safe to re-run. Everything logged to <root>/update.log.
// ──────────────────────────────────────────────────────────────────────────

#include "updater/InstallLayout.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVersionNumber>

#if defined(Q_OS_UNIX)
#include <csignal>  // kill(pid, 0)
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace {

constexpr int   kParentWaitMs   = 15000;  // bounded wait for parent to exit
constexpr int   kPollMs         = 100;
constexpr int   kMaxLaunchTries = 3;      // attempts before rollback
constexpr int   kSettleMs       = 4000;   // grace for the app to clear sentinel
constexpr int   kKeepVersions   = 2;

QString g_logPath;

void logLine(const QString &msg) {
    const QString line = QStringLiteral("%1  [vc-updater] %2\n")
        .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODate), msg);
    if (!g_logPath.isEmpty()) {
        QFile f(g_logPath);
        if (f.open(QIODevice::Append | QIODevice::Text)) {
            f.write(line.toUtf8());
            f.close();
        }
    }
    fputs(line.toLocal8Bit().constData(), stderr);
}

bool processAlive(qint64 pid) {
    if (pid <= 0) return false;
#if defined(Q_OS_WIN)
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!h) return false;
    const DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return r == WAIT_TIMEOUT;  // still running
#else
    return ::kill(static_cast<pid_t>(pid), 0) == 0;
#endif
}

void waitForParent(qint64 pid) {
    if (pid <= 0) return;
    int waited = 0;
    while (waited < kParentWaitMs && processAlive(pid)) {
        QThread::msleep(kPollMs);
        waited += kPollMs;
    }
    if (processAlive(pid))
        logLine(QStringLiteral("parent %1 still alive after %2ms — proceeding anyway")
                    .arg(pid).arg(kParentWaitMs));
    else
        logLine(QStringLiteral("parent %1 exited (waited %2ms)").arg(pid).arg(waited));
}

// Read/increment/reset the small attempt counter stored under a version dir.
int readAttempts(const QString &dir) {
    QFile f(QDir(dir).absoluteFilePath(vcam::attemptsName()));
    if (!f.open(QIODevice::ReadOnly)) return 0;
    const int n = QString::fromUtf8(f.readAll()).trimmed().toInt();
    f.close();
    return n;
}
void writeAttempts(const QString &dir, int n) {
    QFile f(QDir(dir).absoluteFilePath(vcam::attemptsName()));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(n));
        f.close();
    }
}

bool sentinelPresent(const QString &dir) {
    return QFileInfo::exists(QDir(dir).absoluteFilePath(vcam::pendingVerifyName()));
}
void writeSentinel(const QString &dir) {
    QFile f(QDir(dir).absoluteFilePath(vcam::pendingVerifyName()));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QByteArray::number(QDateTime::currentSecsSinceEpoch()));
        f.close();
    }
}

// Launch the new version with --just-updated <ver>, detached. Returns true on
// a successful spawn.
//
// On Linux we MUST go through versions/<v>/viewcam.sh: the ViewCam binary has
// no $ORIGIN/lib RPATH, so a raw exec can't find the bundled Qt on a machine
// without a matching system Qt — exactly the post-update relaunch that used to
// silently fail. viewcam.sh sets LD_LIBRARY_PATH/QT_PLUGIN_PATH/QML_IMPORT_PATH
// and forwards "$@", so the --just-updated sentinel still reaches the app. We
// fall back to the raw exe only if the script is missing (older package).
bool launchApp(const QString &versionDir, const QString &ver) {
    QString target;
    const QString launcher = vcam::appLauncherName();
    if (!launcher.isEmpty()) {
        const QString script = QDir(versionDir).absoluteFilePath(launcher);
        if (QFileInfo::exists(script))
            target = script;
    }
    if (target.isEmpty())
        target = QDir(versionDir).absoluteFilePath(vcam::appExeName());

    if (!QFileInfo::exists(target)) {
        logLine(QStringLiteral("relaunch target missing: %1").arg(target));
        return false;
    }
    qint64 pid = 0;
    const bool ok = QProcess::startDetached(
        target, {QStringLiteral("--just-updated"), ver}, versionDir, &pid);
    if (ok)
        logLine(QStringLiteral("relaunched %1 (pid %2)").arg(target).arg(pid));
    else
        logLine(QStringLiteral("startDetached failed for %1").arg(target));
    return ok;
}

// Keep the newest kKeepVersions version dirs; always pin the active version and
// its rollback target so they're never pruned even if older than the cutoff.
// "Newest" = highest QVersionNumber.
void pruneVersions(const QString &root, const QString &active, const QString &rollback) {
    QDir d(vcam::versionsDir(root));
    QStringList entries = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    std::sort(entries.begin(), entries.end(), [](const QString &a, const QString &b) {
        return QVersionNumber::fromString(a) < QVersionNumber::fromString(b);
    });

    const QString activeName   = QFileInfo(active).fileName();
    const QString rollbackName = QFileInfo(rollback).fileName();

    // Build the keep set: the newest kKeepVersions, plus the two pinned dirs.
    QSet<QString> keep;
    for (int i = entries.size() - 1; i >= 0 && keep.size() < kKeepVersions; --i)
        keep.insert(entries.at(i));
    if (!activeName.isEmpty())   keep.insert(activeName);
    if (!rollbackName.isEmpty()) keep.insert(rollbackName);

    for (const QString &name : entries) {
        if (keep.contains(name)) continue;
        const QString victim = d.absoluteFilePath(name);
        if (QDir(victim).removeRecursively())
            logLine(QStringLiteral("pruned old version %1").arg(name));
        else
            logLine(QStringLiteral("failed to prune %1").arg(victim));
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    const QStringList a = app.arguments();
    if (a.size() < 6) {
        fputs("usage: vc-updater <oldVersionDir> <newVersionDir> <installRoot> "
              "<parentPid> <relaunchPath>\n", stderr);
        return 2;
    }

    const QString oldVersionDir = a.at(1);
    const QString newVersionDir = a.at(2);
    const QString root          = a.at(3);
    const qint64  parentPid     = a.at(4).toLongLong();
    const QString relaunchPath  = a.at(5);  // informational; we derive from current

    g_logPath = vcam::updateLogPath(root);
    logLine(QStringLiteral("=== apply: old=%1 new=%2 root=%3 pid=%4 ===")
                .arg(oldVersionDir, newVersionDir, root).arg(parentPid));

    if (!QFileInfo(QDir(newVersionDir).absoluteFilePath(vcam::appExeName())).exists()) {
        logLine(QStringLiteral("FATAL: new version has no app exe: %1").arg(newVersionDir));
        return 1;
    }

    // 1) Wait for the app to exit so its files are no longer in use (Windows
    //    especially can't replace a running exe).
    waitForParent(parentPid);

    const QString newVer = QFileInfo(newVersionDir).fileName();

    // 2) Activate: repoint current -> versions/<new>.
    QString actErr;
    if (!vcam::activateVersion(root, newVer, &actErr)) {
        logLine(QStringLiteral("activation failed: %1 — aborting, no swap").arg(actErr));
        return 1;
    }
    logLine(QStringLiteral("activated current -> %1").arg(newVer));

    // Record the rollback target so a later run (or the next launch) knows it.
    if (!oldVersionDir.isEmpty()) {
        QFile pf(QDir(root).absoluteFilePath(vcam::previousLinkName()));
        if (pf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            pf.write(oldVersionDir.toUtf8());
            pf.close();
        }
    }

    // 3) First-run sentinel + bounded relaunch loop. The app deletes
    //    .pending-verify on a successful start (main.cpp).
    bool started = false;
    int attempt = readAttempts(newVersionDir);
    while (attempt < kMaxLaunchTries) {
        ++attempt;
        writeAttempts(newVersionDir, attempt);
        writeSentinel(newVersionDir);
        logLine(QStringLiteral("launch attempt %1/%2").arg(attempt).arg(kMaxLaunchTries));

        if (!launchApp(newVersionDir, newVer)) {
            QThread::msleep(kPollMs);
            continue;
        }

        // Give the app a grace period to boot and clear the sentinel.
        int waited = 0;
        while (waited < kSettleMs && sentinelPresent(newVersionDir)) {
            QThread::msleep(kPollMs);
            waited += kPollMs;
        }
        if (!sentinelPresent(newVersionDir)) {
            logLine(QStringLiteral("new version booted OK (sentinel cleared in %1ms)").arg(waited));
            started = true;
            break;
        }
        logLine(QStringLiteral("attempt %1: sentinel still set after %2ms")
                    .arg(attempt).arg(kSettleMs));
    }

    // 4) Rollback if the new version never confirmed a clean start.
    if (!started) {
        logLine(QStringLiteral("new version %1 failed to start after %2 attempts")
                    .arg(newVer).arg(kMaxLaunchTries));
        if (!oldVersionDir.isEmpty() &&
            QFileInfo(QDir(oldVersionDir).absoluteFilePath(vcam::appExeName())).exists()) {
            const QString oldVer = QFileInfo(oldVersionDir).fileName();
            QString rbErr;
            if (vcam::activateVersion(root, oldVer, &rbErr)) {
                logLine(QStringLiteral("ROLLED BACK current -> %1").arg(oldVer));
                writeAttempts(newVersionDir, 0);  // reset so a manual retry is possible
                QFile::remove(QDir(newVersionDir).absoluteFilePath(vcam::pendingVerifyName()));
                // Loop guard: mark this version as launch-failed so the app won't
                // auto-apply it again (which would roll back → re-check → loop).
                QFile ff(QDir(root).absoluteFilePath(vcam::failedVersionName()));
                if (ff.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    ff.write(newVer.toUtf8());
                    ff.close();
                }
                launchApp(oldVersionDir, oldVer);  // bring the working version back up
            } else {
                logLine(QStringLiteral("rollback activation FAILED: %1").arg(rbErr));
            }
        } else {
            logLine(QStringLiteral("no usable previous version to roll back to — "
                                   "leaving current at %1").arg(newVer));
        }
        return 1;
    }

    // 5) Prune old versions (keep the new + its rollback target, max 2).
    pruneVersions(root, newVersionDir, oldVersionDir);

    logLine(QStringLiteral("=== apply complete: now on %1 ===").arg(newVer));
    return 0;
}
