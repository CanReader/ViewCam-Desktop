import QtQuick
import QtQuick.Controls
import ViewCam.Studio

// Full-window Pro gate overlay — shown when a non-Pro user tries to interact
// with a Pro-gated setting. Use as a Popup anchored to the window overlay.
Popup {
    id: root

    anchors.centerIn: Overlay.overlay
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    signal upgradeRequested()

    background: Rectangle {
        color: "transparent"
    }

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.55)
    }

    // Card
    Rectangle {
        width: 360
        height: col.implicitHeight + 48
        radius: Theme.radiusXl
        color: Theme.bg2
        border.width: 1
        border.color: Theme.line2

        Column {
            id: col
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 28
            spacing: 0
            width: parent.width - 48

            // PRO badge
            VcProBadge {
                anchors.horizontalCenter: parent.horizontalCenter
                implicitWidth: 52
                implicitHeight: 26
            }

            // Title
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("ViewCam Pro")
                font.family: Theme.fontSans
                font.pixelSize: 22
                font.weight: Font.DemiBold
                font.letterSpacing: -0.01 * 22
                color: Theme.fg1
                topPadding: 14
            }

            // Subtitle
            Text {
                width: parent.width
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Unlock advanced features for the best experience.")
                font.family: Theme.fontSans
                font.pixelSize: 13
                color: Theme.fg2
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                topPadding: 8
            }

            // Feature bullets
            Item { width: 1; height: 20 }
            Repeater {
                model: [
                    qsTr("4K & 1080p output resolution"),
                    qsTr("GPU-accelerated processing"),
                    qsTr("Advanced codecs: H.264 & H.265"),
                    qsTr("Pro audio features"),
                ]
                Row {
                    spacing: 10
                    topPadding: 7
                    width: parent.width

                    Rectangle {
                        width: 6; height: 6
                        radius: 3
                        color: Theme.iris
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.verticalCenterOffset: 3
                    }
                    Text {
                        text: modelData
                        font.family: Theme.fontSans
                        font.pixelSize: 13
                        color: Theme.fg2
                    }
                }
            }

            // Buttons
            Item { width: 1; height: 24 }
            VcButton {
                width: parent.width
                kind: "primary"
                text: qsTr("Upgrade to Pro")
                onClicked: {
                    root.upgradeRequested()
                    root.close()
                }
            }
            Item { width: 1; height: 10 }
            VcButton {
                width: parent.width
                kind: "soft"
                text: qsTr("Not now")
                onClicked: root.close()
            }
        }
    }

    // Size popup to match card content
    implicitWidth: 360
    implicitHeight: col.implicitHeight + 48
}
