#pragma once
#include <QObject>
#include <QTimer>

class QQmlApplicationEngine;
class QFileSystemWatcher;

// QML hot-reload — active only in Debug builds (VIEWCAM_HOT_RELOAD defined).
//
// Instead of a staging qmldir, installs a QQmlAbstractUrlInterceptor that
// redirects every rcc-based .qml URL (qrc:/qt/qml/ViewCam/Studio/qml/…) to
// the corresponding on-disk source file. The rcc is still registered (fonts,
// images, icons load from it); only the .qml files are sourced from disk.
//
// On any .qml/.js save: clearComponentCache() + delete old root + reload.
// C++ singletons (AppController, AppInfo, …) survive across reloads.
//
// In Release builds (VIEWCAM_HOT_RELOAD not defined), install() is a no-op
// compiled away and every call costs nothing.
class QmlDevMode : public QObject {
    Q_OBJECT
public:
    explicit QmlDevMode(QObject *parent = nullptr);

    // True only when VIEWCAM_HOT_RELOAD is defined (Debug build).
    static bool active();

    // On-disk qml/ source directory. Debug builds bake VIEWCAM_QML_SOURCE_DIR
    // from CMake; VIEWCAM_QML_DIR env var overrides at runtime if set.
    static QString sourceDir();

    // Install the URL interceptor and file watcher before loadFromModule().
    // No-op (instant return) in Release builds.
    void install(QQmlApplicationEngine *engine);

private slots:
    void onFileChanged(const QString &path);
    void onDirectoryChanged(const QString &path);
    void doReload();

private:
    void watchRecursive(const QString &dir);

    QQmlApplicationEngine *m_engine    = nullptr;
    QFileSystemWatcher    *m_watcher   = nullptr;
    QTimer                 m_debounce;
    QString                m_sourceDir;
};
