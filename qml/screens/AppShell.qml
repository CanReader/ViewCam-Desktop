import QtQuick
import ViewCam.Studio

// Main shell: sidebar (280) | center page | right panel (344, Live View
// only — matches the mockup's data-page grid behavior).
Item {
    id: root

    property string page: "liveview"
    signal openLauncher()

    // Restore the active page from the C++ singleton on creation.
    // AppController survives hot-reload, so the user lands on the same screen.
    Component.onCompleted: page = AppController.activePage

    // Keep the C++ singleton in sync so the page survives the next reload.
    onPageChanged: AppController.activePage = page

    Sidebar {
        id: sidebar
        anchors.left: parent.left
        width: Theme.sidebarW
        height: parent.height
        page: root.page
        onNavigate: (p) => root.page = p
    }

    // center pages — fadeUp entrance per the mockup (.page.is-active)
    Item {
        anchors.left: sidebar.right
        anchors.right: rightPanel.visible ? rightPanel.left : parent.right
        height: parent.height

        LiveView {
            id: liveView
            anchors.fill: parent
            visible: root.page === "liveview"
            onOpenLauncher: root.openLauncher()
            onVisibleChanged: if (visible) enterLive.restart()

            ParallelAnimation {
                id: enterLive
                NumberAnimation {
                    target: liveView; property: "opacity"; from: 0; to: 1
                    duration: Theme.durPull
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easeIris
                }
            }
        }

        SourcesPage {
            id: sourcesPage
            anchors.fill: parent
            visible: root.page === "sources"
            onVisibleChanged: if (visible) enterSources.restart()

            ParallelAnimation {
                id: enterSources
                NumberAnimation {
                    target: sourcesPage; property: "opacity"; from: 0; to: 1
                    duration: Theme.durPull
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easeIris
                }
            }
        }

        SettingsPage {
            id: settingsPage
            anchors.fill: parent
            visible: root.page === "settings"
            onVisibleChanged: if (visible) enterSettings.restart()

            ParallelAnimation {
                id: enterSettings
                NumberAnimation {
                    target: settingsPage; property: "opacity"; from: 0; to: 1
                    duration: Theme.durPull
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: Theme.easeIris
                }
            }
        }
    }

    RightPanel {
        id: rightPanel
        anchors.right: parent.right
        width: Theme.rightPanelW
        height: parent.height
        // stats/controls belong to a live device — hide when nothing is connected
        // so the empty viewfinder goes full-bleed instead of showing dead "—" rows.
        visible: root.page === "liveview" && AppController.connection.connected
    }
}
