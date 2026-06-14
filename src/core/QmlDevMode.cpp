#include "core/QmlDevMode.h"
#include "core/Logger.h"

// Hot-reload is a Debug-only feature. All implementation is guarded so the
// Release binary compiles to three tiny stubs with zero overhead.
#ifdef VIEWCAM_HOT_RELOAD

#include <QDir>
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QMetaObject>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlApplicationEngine>
#include <QUrl>

// Intercepts every QML / JS URL that belongs to our rcc module and redirects
// it to the on-disk source file. Everything else (fonts, images, qmldirs, Qt
// built-in QML imports) passes through unchanged.
class DevUrlInterceptor : public QQmlAbstractUrlInterceptor {
public:
    // rccPrefix  — "qrc:/qt/qml/ViewCam/Studio/qml/" (the path in the bundle)
    // sourceDir  — absolute path to Desktop/qml/ on disk
    DevUrlInterceptor(const QString &rccPrefix, const QString &sourceDir)
        : m_rccPrefix(rccPrefix), m_sourceDir(sourceDir) {}

    QUrl intercept(const QUrl &url, DataType type) override {
        // Only redirect .qml and .js files; let qmldirs and other assets pass.
        if (type != QmlFile && type != JavaScriptFile)
            return url;

        const QString s = url.toString();
        if (!s.startsWith(m_rccPrefix))
            return url;

        // e.g. "components/VcCard.qml" or "screens/AppShell.qml" or "Main.qml"
        const QString relative = s.mid(m_rccPrefix.length());
        const QUrl fileUrl = QUrl::fromLocalFile(m_sourceDir + "/" + relative);
        return fileUrl;
    }

private:
    QString m_rccPrefix;
    QString m_sourceDir;
};

#endif // VIEWCAM_HOT_RELOAD

// ── static helpers ────────────────────────────────────────────────────────────

bool QmlDevMode::active() {
#ifdef VIEWCAM_HOT_RELOAD
    return true;
#else
    return false;
#endif
}

QString QmlDevMode::sourceDir() {
#ifdef VIEWCAM_HOT_RELOAD
    // VIEWCAM_QML_DIR env override → fallback to CMake-baked absolute path.
    const QString envDir = qEnvironmentVariable("VIEWCAM_QML_DIR");
    if (!envDir.isEmpty())
        return QDir::cleanPath(envDir);
    // VIEWCAM_QML_SOURCE_DIR is defined by CMake to ${CMAKE_CURRENT_SOURCE_DIR}/qml
    return QStringLiteral(VIEWCAM_QML_SOURCE_DIR);
#else
    return {};
#endif
}

// ── construction ──────────────────────────────────────────────────────────────

QmlDevMode::QmlDevMode(QObject *parent) : QObject(parent) {
#ifdef VIEWCAM_HOT_RELOAD
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(200);
    connect(&m_debounce, &QTimer::timeout, this, &QmlDevMode::doReload);
#endif
}

// ── install ───────────────────────────────────────────────────────────────────

void QmlDevMode::install(QQmlApplicationEngine *engine) {
#ifdef VIEWCAM_HOT_RELOAD
    m_engine    = engine;
    m_sourceDir = sourceDir();

    if (!QDir(m_sourceDir).exists()) {
        VC_WARN("QML hot-reload: source dir not found: {}", m_sourceDir.toStdString());
        VC_WARN("  Set VIEWCAM_QML_DIR to override the baked path.");
        return;
    }

    // The rcc stores our QML under this prefix.
    // Must match VIEWCAM_RCC_URI_PATH in cmake/ViewCamBundle.cmake + the "qml/" subdir.
    const QString rccPrefix = QStringLiteral("qrc:/qt/qml/ViewCam/Studio/qml/");

    // Install interceptor; the engine takes ownership.
    engine->addUrlInterceptor(new DevUrlInterceptor(rccPrefix, m_sourceDir));

    // Watch all .qml/.js files in the source tree.
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &QmlDevMode::onFileChanged);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &QmlDevMode::onDirectoryChanged);
    watchRecursive(m_sourceDir);

    VC_INFO("QML hot-reload: ACTIVE — watching {}", m_sourceDir.toStdString());
#else
    Q_UNUSED(engine)
#endif
}

// ── watcher ───────────────────────────────────────────────────────────────────

#ifdef VIEWCAM_HOT_RELOAD

void QmlDevMode::watchRecursive(const QString &dir) {
    m_watcher->addPath(dir);
    QDirIterator subdirs(dir, QDir::Dirs | QDir::NoDotAndDotDot,
                         QDirIterator::Subdirectories);
    while (subdirs.hasNext())
        m_watcher->addPath(subdirs.next());

    QDirIterator files(dir, {"*.qml", "*.js"}, QDir::Files,
                       QDirIterator::Subdirectories);
    while (files.hasNext())
        m_watcher->addPath(files.next());
}

void QmlDevMode::onFileChanged(const QString &path) {
    // Some editors (vim, VSCode) write atomically; re-add to restore the watch.
    m_watcher->addPath(path);
    m_debounce.start();
}

void QmlDevMode::onDirectoryChanged(const QString &path) {
    watchRecursive(path); // pick up any newly-created files
    m_debounce.start();
}

void QmlDevMode::doReload() {
    VC_INFO("QML hot-reload: reloading content...");
    if (m_engine->rootObjects().isEmpty()) {
        VC_WARN("QML hot-reload: no root window — skipping");
        return;
    }

    // The Window (Main.qml) is PERSISTENT and is NOT recreated.
    // We only reload the inner Loader (AppContent.qml) so the window never
    // disappears, raises itself, or steals focus.
    QObject *win = m_engine->rootObjects().first();

    // Clear the component cache so edited .qml files are re-parsed from disk.
    m_engine->clearComponentCache();

    // Ask the persistent window to reset its Loader source.
    // reloadContent() sets loader.source="" then back to AppContent.qml —
    // navigation state is preserved via AppController.activePage (C++ side).
    QMetaObject::invokeMethod(win, "reloadContent", Qt::DirectConnection);

    VC_INFO("QML hot-reload: done");
}

#else // stub slots so the moc-generated code compiles in Release

void QmlDevMode::onFileChanged(const QString &) {}
void QmlDevMode::onDirectoryChanged(const QString &) {}
void QmlDevMode::doReload() {}
void QmlDevMode::watchRecursive(const QString &) {}

#endif
