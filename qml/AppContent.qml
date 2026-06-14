import QtQuick
import ViewCam.Studio

// Reloadable app content — everything inside the persistent window.
// Loaded via a Loader in Main.qml; hot-reload only recreates this item.
// Navigation page and launcher open-state are restored from the C++
// AppController singleton so the user stays on the same screen after a reload.
Item {
    id: root

    AppShell {
        id: shell
        anchors.fill: parent
        onOpenLauncher: launcher.open = true
    }

    Launcher {
        id: launcher
        anchors.fill: parent
        open: true  // Component.onCompleted overrides from C++ connection state
    }

    Component.onCompleted: {
        // Restore UI state from persistent C++ singletons.
        // AppController.activePage and AppController.connection.connected
        // both survive hot-reload, so the user lands on the same screen.
        launcher.open = !AppController.connection.connected;
        // AppShell reads activePage in its own Component.onCompleted.
    }

    // Connecting hides the launcher and lands on the stored active page.
    Connections {
        target: AppController.connection
        function onStateChanged() {
            if (AppController.connection.connected) {
                launcher.open = false;
                shell.page = AppController.activePage;
            }
        }
    }
}
