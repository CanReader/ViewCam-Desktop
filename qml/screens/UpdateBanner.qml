import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Non-blocking "Update available" prompt (Phase 5). A small card pinned to the
// bottom of the app shell — the app stays fully usable behind it. Design-system
// styled (flat charcoal, no grain, Vc* components). Also owns the periodic
// background re-check timer and keeps UpdateChecker's auto-download flag in sync
// with the persisted setting.
Item {
    id: root

    readonly property var s: AppController.settings

    // Surface the card on UpdateAvailable, or while a user-chosen "Install on
    // quit" download is in flight / verified (so the user gets confirmation the
    // update will apply on exit). Hidden once dismissed via "Later".
    property bool dismissed: false

    readonly property bool quitFlow:
           UpdateChecker.isApplyPendingOnQuit
        && (UpdateChecker.state === UpdateChecker.Downloading
         || UpdateChecker.state === UpdateChecker.Verifying
         || UpdateChecker.state === UpdateChecker.ReadyToApply)

    // Banner intentionally disabled: updates apply SILENTLY (auto-update) with the
    // progress overlay, no prompt card. This Item still hosts the periodic re-check
    // timer + auto-download sync below. (Was: visible on UpdateAvailable.)
    readonly property bool show: false

    // Re-surface on every fresh check result.
    Connections {
        target: UpdateChecker
        function onStateChanged() {
            if (UpdateChecker.state === UpdateChecker.UpdateAvailable)
                root.dismissed = false
        }
    }

    // Keep the C++ auto-download flag aligned with the persisted setting.
    Component.onCompleted: UpdateChecker.setAutoDownload(root.s.autoUpdate)
    Connections {
        target: root.s
        function onAutoUpdateChanged() {
            UpdateChecker.setAutoDownload(root.s.autoUpdate)
        }
    }

    // Periodic background re-check. Frequency: 0 = On launch only (no timer),
    // 1 = Daily, 2 = Weekly. The initial check is kicked off from main().
    Timer {
        id: periodic
        readonly property int dayMs: 24 * 60 * 60 * 1000
        interval: root.s.updateFrequency === 2 ? 7 * dayMs : dayMs
        repeat: true
        running: root.s.updateFrequency !== 0
        onTriggered: UpdateChecker.checkNow()
    }

    visible: card.opacity > 0
    implicitHeight: card.height

    Rectangle {
        id: card
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        width: Math.min(420, parent.width - 40)
        implicitHeight: inner.implicitHeight + 36
        height: implicitHeight

        radius: Theme.radiusXl
        color: Theme.bg2
        border.width: 1
        border.color: Theme.line1

        opacity: root.show ? 1 : 0
        y: root.show ? 0 : 16
        Behavior on opacity { NumberAnimation { duration: Theme.durPull; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeIris } }
        Behavior on y       { NumberAnimation { duration: Theme.durPull; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easeIris } }

        // Soft iris accent rail on the leading edge.
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
            width: 3
            radius: 2
            color: Theme.iris
        }

        ColumnLayout {
            id: inner
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 18
            anchors.topMargin: 18
            anchors.bottomMargin: 18
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 36
                    Layout.preferredHeight: 36
                    radius: Theme.radiusMd
                    color: Theme.irisSoft
                    VcIcon {
                        anchors.centerIn: parent
                        width: 18; height: 18
                        name: "reconnect"
                        color: Theme.iris
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        text: root.quitFlow
                              ? qsTr("Update %1 will install on quit").arg(UpdateChecker.latestVersion)
                              : qsTr("Update %1 available").arg(UpdateChecker.latestVersion)
                        font.family: Theme.fontSans
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: Theme.fg1
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: root.quitFlow
                              ? UpdateChecker.statusText
                              : qsTr("A new version of %1 is ready to download.").arg(AppInfo.displayName)
                        font.family: Theme.fontSans
                        font.pixelSize: 12
                        color: Theme.fg3
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Release notes link.
            Text {
                visible: UpdateChecker.notesUrl !== ""
                text: qsTr("Release notes")
                font.family: Theme.fontSans
                font.pixelSize: 12
                font.weight: Font.Medium
                font.underline: notesHover.hovered
                color: Theme.iris
                HoverHandler { id: notesHover }
                TapHandler { onTapped: Qt.openUrlExternally(UpdateChecker.notesUrl) }
            }

            // Actions. While the quit-flow download runs, collapse to a single
            // "Cancel" affordance + a thin progress bar.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: !root.quitFlow

                VcButton {
                    kind: "primary"
                    text: qsTr("Install on quit")
                    // Default action: download in background now; apply on exit.
                    onClicked: {
                        UpdateChecker.installOnQuit()
                        UpdateChecker.download()
                    }
                }
                VcButton {
                    kind: "soft"
                    text: qsTr("Install now")
                    // Download → ReadyToApply → apply() immediately (see watcher).
                    onClicked: {
                        root.installNowArmed = true
                        UpdateChecker.cancelInstallOnQuit()
                        UpdateChecker.download()
                    }
                }
                Item { Layout.fillWidth: true }
                VcButton {
                    kind: "soft"
                    text: qsTr("Later")
                    onClicked: root.dismissed = true
                }
            }

            // Quit-flow status: progress while downloading/verifying, then a
            // "ready — applies on quit" confirmation with a Cancel.
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                visible: root.quitFlow

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 3
                    radius: 2
                    color: Theme.line2
                    visible: UpdateChecker.state === UpdateChecker.Downloading
                    Rectangle {
                        width: parent.width * (UpdateChecker.progress / 100.0)
                        height: parent.height
                        radius: 2
                        color: Theme.iris
                        Behavior on width { NumberAnimation { duration: 80 } }
                    }
                }
                Item {
                    Layout.fillWidth: true
                    visible: UpdateChecker.state !== UpdateChecker.Downloading
                }
                VcButton {
                    kind: "soft"
                    text: qsTr("Cancel")
                    onClicked: {
                        UpdateChecker.cancelInstallOnQuit()
                        root.dismissed = true
                    }
                }
            }
        }
    }

    // "Install now": when the user picked it, apply as soon as the verified
    // archive reaches ReadyToApply.
    property bool installNowArmed: false
    Connections {
        target: UpdateChecker
        function onStateChanged() {
            if (root.installNowArmed
                && UpdateChecker.state === UpdateChecker.ReadyToApply) {
                root.installNowArmed = false
                UpdateChecker.apply()
            }
        }
    }
}
