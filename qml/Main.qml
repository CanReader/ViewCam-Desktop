import QtQuick
import ViewCam.Studio

Window {
    id: win

    width: 1440
    height: 900
    minimumWidth: 1080
    minimumHeight: 640
    visible: true
    title: AppInfo.displayName
    color: Theme.bg1

    AppShell {
        id: shell
        anchors.fill: parent
        onOpenLauncher: launcher.open = true
    }

    Launcher {
        id: launcher
        anchors.fill: parent
        open: true
    }

    // connecting hides the launcher and lands on Live View
    Connections {
        target: AppController.connection
        function onStateChanged() {
            if (AppController.connection.connected) {
                launcher.open = false;
                shell.page = "liveview";
            }
        }
    }
}
