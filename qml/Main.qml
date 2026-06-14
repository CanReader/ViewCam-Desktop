import QtQuick
import ViewCam.Studio

// Persistent shell window — created ONCE and never reloaded.
// Only the inner Loader (AppContent.qml) is swapped on hot-reload.
// Geometry, visibility, and focus are never touched during a reload.
Window {
    id: win

    width: 1440
    height: 900
    minimumWidth: 1080
    minimumHeight: 640
    visible: true
    title: AppInfo.displayName
    color: Theme.bg1

    Loader {
        id: contentLoader
        anchors.fill: parent
        source: Qt.resolvedUrl("AppContent.qml")
    }

    // Called by QmlDevMode.doReload() — only the inner content is reset.
    // The Window (geometry, visibility, raise state, focus) is untouched.
    function reloadContent() {
        contentLoader.source = "";
        contentLoader.source = Qt.resolvedUrl("AppContent.qml");
    }
}
