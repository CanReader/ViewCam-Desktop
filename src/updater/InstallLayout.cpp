#include "updater/InstallLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "miniz.h"

#if defined(Q_OS_UNIX)
#include <cerrno>
#include <cstring>
#include <unistd.h>  // symlink, readlink
#endif

namespace vcam {

QString installRoot() {
    // Brief §1: Linux ~/.local/share/ViewCam, Windows %LOCALAPPDATA%\ViewCam.
    // MUST be deterministic and identical to where install.sh lays the layout
    // down. Do NOT use QStandardPaths::AppDataLocation — it appends the app
    // identity (org "Sleak Software" / app "ViewCam") → ~/.local/share/Sleak
    // Software/ViewCam, which does NOT match install.sh's ~/.local/share/ViewCam,
    // so isSelfUpdatable() would deny and the helper would target the wrong root.
    // GenericDataLocation is the org/app-free base ($XDG_DATA_HOME, default
    // ~/.local/share) — append a fixed "ViewCam".
    QString base;
#if defined(Q_OS_WIN)
    const QByteArray local = qgetenv("LOCALAPPDATA");
    if (!local.isEmpty())
        base = QDir(QString::fromLocal8Bit(local)).absoluteFilePath(QStringLiteral("ViewCam"));
#endif
    if (base.isEmpty()) {
        const QString generic =
            QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (!generic.isEmpty())
            base = QDir(generic).absoluteFilePath(QStringLiteral("ViewCam"));
        else
            base = QDir(QDir::homePath()).absoluteFilePath(QStringLiteral(".local/share/ViewCam"));
    }
    return QDir::cleanPath(base);
}

QString versionsDir(const QString &root) {
    return QDir(root).absoluteFilePath(QStringLiteral("versions"));
}
QString versionDir(const QString &root, const QString &ver) {
    return QDir(versionsDir(root)).absoluteFilePath(ver);
}
QString currentLink(const QString &root) {
    return QDir(root).absoluteFilePath(QStringLiteral("current"));
}
QString updateLogPath(const QString &root) {
    return QDir(root).absoluteFilePath(QStringLiteral("update.log"));
}

QString resolveCurrent(const QString &root) {
    const QString link = currentLink(root);
    QFileInfo fi(link);
#if defined(Q_OS_WIN)
    // Prefer the pointer file; fall back to a junction that QFileInfo resolves.
    const QString ptr = QDir(root).absoluteFilePath(QStringLiteral("current.txt"));
    QFile pf(ptr);
    if (pf.exists() && pf.open(QIODevice::ReadOnly)) {
        const QString target = QString::fromUtf8(pf.readAll()).trimmed();
        pf.close();
        if (!target.isEmpty()) {
            QString abs = target;
            if (QFileInfo(abs).isRelative())
                abs = QDir(root).absoluteFilePath(abs);
            return QDir::cleanPath(abs);
        }
    }
    if (fi.isSymLink() || fi.isJunction())
        return QDir::cleanPath(fi.symLinkTarget());
    if (fi.exists())
        return QDir::cleanPath(fi.absoluteFilePath());
    return {};
#else
    if (fi.isSymLink())
        return QDir::cleanPath(fi.symLinkTarget());
    if (fi.exists())  // tolerate a plain dir if something replaced the symlink
        return QDir::cleanPath(fi.absoluteFilePath());
    return {};
#endif
}

bool isSelfUpdatable(const QString &root, const QString &runningExePath,
                     QString *reason) {
    auto deny = [&](const QString &why) {
        if (reason) *reason = why;
        return false;
    };

#if defined(Q_OS_WIN)
    // Windows ships a SYSTEM virtual camera (registered DLLs in Program Files,
    // needs admin). The update unit is the signed Setup.exe, which self-elevates
    // and upgrades in place. So Windows is ALWAYS self-updatable regardless of
    // where the binary lives or whether the per-user root is writable — the
    // installer, not us, owns the apply step.
    Q_UNUSED(root);
    Q_UNUSED(runningExePath);
    return true;
#else

    if (root.isEmpty())
        return deny(QStringLiteral("install root could not be resolved"));

    const QFileInfo rootFi(root);
    if (!rootFi.exists())
        return deny(QStringLiteral("install root does not exist (fresh/packaged install)"));
    if (!rootFi.isWritable())
        return deny(QStringLiteral("install root is not writable"));

    // If we know where the running binary lives, it MUST be under the per-user
    // root. A binary under /opt, /usr, Program Files, /app (Flatpak), etc. is a
    // system/packaged install → notify only, never swap.
    if (!runningExePath.isEmpty()) {
        const QString exe  = QDir::cleanPath(runningExePath);
        const QString base = QDir::cleanPath(root) + QLatin1Char('/');
        if (!exe.startsWith(base))
            return deny(QStringLiteral("running outside the per-user install root"));
    }

    // Confirm we can actually create a file at the root (catches read-only
    // mounts that lie about isWritable, and missing dirs).
    QDir().mkpath(root);
    const QString probe = QDir(root).absoluteFilePath(QStringLiteral(".write-probe"));
    QFile pf(probe);
    if (!pf.open(QIODevice::WriteOnly)) {
        return deny(QStringLiteral("install root rejected a write probe"));
    }
    pf.close();
    QFile::remove(probe);
    return true;
#endif  // Q_OS_WIN
}

bool activateVersion(const QString &root, const QString &ver, QString *err) {
    const QString target = versionDir(root, ver);
    if (!QFileInfo(target).isDir()) {
        if (err) *err = QStringLiteral("version dir missing: %1").arg(target);
        return false;
    }
    const QString link = currentLink(root);

#if defined(Q_OS_WIN)
    // Pointer file is the source of truth (junctions need privileges on some
    // configs). Write atomically: temp + ReplaceFile-style rename.
    const QString ptr    = QDir(root).absoluteFilePath(QStringLiteral("current.txt"));
    const QString tmpPtr = ptr + QStringLiteral(".tmp");
    {
        QFile f(tmpPtr);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (err) *err = QStringLiteral("cannot write current.txt.tmp");
            return false;
        }
        f.write(target.toUtf8());
        f.flush();
        f.close();
    }
    QFile::remove(ptr);
    if (!QFile::rename(tmpPtr, ptr)) {
        if (err) *err = QStringLiteral("cannot replace current.txt");
        QFile::remove(tmpPtr);
        return false;
    }
    // Best-effort junction so file:// paths / shortcuts to <root>/current work.
    QFile::remove(link);  // remove a stale junction/dir if present
    QFile::link(target, link);  // ignore failure — pointer file already updated
    return true;
#else
    // POSIX: create a temp symlink then rename() it over `current` — rename of
    // a symlink is atomic, so a reader never sees a half-updated link. Use a
    // RELATIVE target ("versions/<v>") so the install survives being moved.
    const QString relTarget = QStringLiteral("versions/") + ver;
    const QString tmpLink = link + QStringLiteral(".new");
    ::unlink(tmpLink.toLocal8Bit().constData());  // drop any stale temp
    if (::symlink(relTarget.toLocal8Bit().constData(),
                  tmpLink.toLocal8Bit().constData()) != 0) {
        if (err) *err = QStringLiteral("symlink() failed: %1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (::rename(tmpLink.toLocal8Bit().constData(),
                 link.toLocal8Bit().constData()) != 0) {
        if (err) *err = QStringLiteral("rename() over current failed: %1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno)));
        ::unlink(tmpLink.toLocal8Bit().constData());
        return false;
    }
    return true;
#endif
}

bool extractZip(const QString &zipPath, const QString &destDir, QString *err) {
    if (!QDir().mkpath(destDir)) {
        if (err) *err = QStringLiteral("cannot create dest dir: %1").arg(destDir);
        return false;
    }
    const QString destCanon = QDir::cleanPath(QDir(destDir).absolutePath()) + QLatin1Char('/');

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, zipPath.toLocal8Bit().constData(), 0)) {
        if (err) *err = QStringLiteral("cannot open zip: %1").arg(zipPath);
        return false;
    }

    bool ok = true;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for (mz_uint i = 0; i < count && ok; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            if (err) *err = QStringLiteral("stat failed for entry %1").arg(i);
            ok = false;
            break;
        }

        const QString name = QString::fromUtf8(st.m_filename);
        // Zip-slip guard: the resolved path must stay strictly under destDir.
        const QString outPath =
            QDir::cleanPath(QDir(destDir).absoluteFilePath(name));
        if (!(outPath + QLatin1Char('/')).startsWith(destCanon)) {
            if (err) *err = QStringLiteral("zip entry escapes dest: %1").arg(name);
            ok = false;
            break;
        }

        if (mz_zip_reader_is_file_a_directory(&zip, i) ||
            name.endsWith(QLatin1Char('/'))) {
            QDir().mkpath(outPath);
            continue;
        }

        QDir().mkpath(QFileInfo(outPath).absolutePath());
        if (!mz_zip_reader_extract_to_file(&zip, i,
                                           outPath.toLocal8Bit().constData(), 0)) {
            if (err) *err = QStringLiteral("extract failed: %1").arg(name);
            ok = false;
            break;
        }

#if defined(Q_OS_UNIX)
        // .zip stores no reliable Unix mode for our toolchain; force +x on the
        // executables so the relaunched app / helper are runnable.
        const QString base = QFileInfo(outPath).fileName();
        if (base == QStringLiteral("ViewCam") || base == QStringLiteral("vc-updater")) {
            QFile::setPermissions(
                outPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                             QFileDevice::ExeOwner | QFileDevice::ReadGroup |
                             QFileDevice::ExeGroup | QFileDevice::ReadOther |
                             QFileDevice::ExeOther);
        }
#endif
    }

    mz_zip_reader_end(&zip);
    return ok;
}

bool repairCurrentSymlinkIfDangling(const QString &root,
                                    const QString &runningExePath) {
#if defined(Q_OS_WIN)
    // Windows uses current.txt + a best-effort junction, not a POSIX symlink,
    // and the activation path is the installer — nothing to self-heal here.
    Q_UNUSED(root);
    Q_UNUSED(runningExePath);
    return false;
#else
    if (root.isEmpty() || runningExePath.isEmpty())
        return false;

    const QString link = currentLink(root);
    const QFileInfo linkFi(link);

    // Only act on a symlink that is broken (target gone). A healthy symlink, a
    // real dir, or an absent link are all left alone.
    if (!linkFi.isSymLink())
        return false;
    if (QFileInfo::exists(linkFi.symLinkTarget()))
        return false;  // points somewhere valid — nothing to heal

    // Infer the version this binary is running from: <root>/versions/<ver>/ViewCam.
    const QString exe   = QDir::cleanPath(runningExePath);
    const QString vBase = QDir::cleanPath(versionsDir(root)) + QLatin1Char('/');
    if (!exe.startsWith(vBase))
        return false;  // not a versioned per-user install; can't infer a target

    const QString rest = exe.mid(vBase.size());  // "<ver>/ViewCam"
    const QString ver  = rest.section(QLatin1Char('/'), 0, 0);
    if (ver.isEmpty())
        return false;
    if (!QFileInfo(versionDir(root, ver)).isDir())
        return false;

    QString err;
    // activateVersion() rewrites current atomically (temp symlink + rename).
    return activateVersion(root, ver, &err);
#endif
}

}  // namespace vcam
