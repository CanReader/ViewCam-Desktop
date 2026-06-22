#pragma once

// ──────────────────────────────────────────────────────────────────────────
// InstallLayout — shared (app + vc-updater helper) helper for the per-user
// versioned install layout described in UPDATE_SYSTEM_BRIEF.md §1:
//
//   <root>/                      Linux ~/.local/share/ViewCam, Win %LOCALAPPDATA%\ViewCam
//     current      -> versions/<v>   (Linux: symlink; Windows: junction or current.txt)
//     versions/<v>/                  full package (ViewCam(.exe)+viewcam.rcc+lib/...)
//     staging/                       extraction scratch
//     vc-updater(.exe)               helper, lives at root
//     update.log
//
// Pure QtCore + miniz; NO Qt Gui/Qml so the standalone helper can link it too.
// ──────────────────────────────────────────────────────────────────────────

#include <QString>

namespace vcam {

// Name of the per-platform executable inside a version dir.
inline QString appExeName() {
#if defined(Q_OS_WIN)
    return QStringLiteral("ViewCam.exe");
#else
    return QStringLiteral("ViewCam");
#endif
}

// Name of the updater helper at the install root.
inline QString helperExeName() {
#if defined(Q_OS_WIN)
    return QStringLiteral("vc-updater.exe");
#else
    return QStringLiteral("vc-updater");
#endif
}

// Sentinel written before the post-update relaunch; the app deletes it on a
// successful start. If still present after N attempts → rollback.
inline QString pendingVerifyName()  { return QStringLiteral(".pending-verify"); }
inline QString attemptsName()       { return QStringLiteral(".launch-attempts"); }
inline QString previousLinkName()   { return QStringLiteral(".previous"); }

// Per-user install root (does NOT create it):
//   Linux:   ~/.local/share/ViewCam
//   Windows: %LOCALAPPDATA%\ViewCam
// Empty string only if the platform paths can't be resolved at all.
QString installRoot();

// Convenience joins under installRoot().
QString versionsDir(const QString &root);                       // <root>/versions
QString versionDir(const QString &root, const QString &ver);    // <root>/versions/<ver>
QString currentLink(const QString &root);                       // <root>/current
QString updateLogPath(const QString &root);                     // <root>/update.log

// Resolve where <root>/current currently points (a version dir), following the
// Linux symlink or reading the Windows current.txt / junction. Empty if unset.
QString resolveCurrent(const QString &root);

// True when self-update is allowed: the running binary lives under `root` AND
// `root` is writable. False for system/packaged installs (/opt, /usr, Program
// Files, Flatpak, read-only dir) — caller must fall back to "update via
// installer". `reason` (optional) is filled with a human-readable cause.
//
// `runningExePath` is the absolute path of the currently-running executable
// (QCoreApplication::applicationFilePath()); pass {} in the helper, which is
// always at the root and only needs the writability check.
bool isSelfUpdatable(const QString &root, const QString &runningExePath,
                     QString *reason = nullptr);

// Atomically repoint <root>/current at versions/<ver>.
//   Linux:   create a temp symlink then rename() it over `current`.
//   Windows: rewrite current.txt (atomic temp+replace); junction is best-effort.
// Returns false on failure; never leaves `current` in a half-written state.
bool activateVersion(const QString &root, const QString &ver, QString *err = nullptr);

// Extract a .zip archive into destDir (created if needed). On POSIX the entry
// 'ViewCam' (and 'vc-updater') get +x. Returns false + fills err on any failure.
// Zip-slip safe: refuses entries that escape destDir.
bool extractZip(const QString &zipPath, const QString &destDir, QString *err = nullptr);

}  // namespace vcam
