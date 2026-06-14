import QtQuick
import ViewCam.Studio

// Manual-IP input: mono text field with an iris "go" button.
// Focus ring = inset border steps to iris.
Rectangle {
    id: root

    property alias text: input.text
    property string placeholder: "192.168.1.45"
    property bool large: false
    signal submitted(string ip)

    implicitHeight: large ? 46 : 40
    radius: Theme.radiusLg
    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(0, 0, 0, 0.03)
    border.width: 1
    border.color: input.activeFocus ? Theme.iris : Theme.line2

    Behavior on border.color {
        ColorAnimation { duration: Theme.durSnap }
    }

    TextInput {
        id: input
        anchors.left: parent.left
        anchors.right: goBtn.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 12
        anchors.rightMargin: 8
        font.family: Theme.fontMono
        font.pixelSize: root.large ? 14 : 12.5
        color: Theme.fg1
        clip: true
        selectionColor: Theme.irisGlow
        inputMethodHints: Qt.ImhPreferNumbers
        onAccepted: root.submitted(text)

        Text {
            anchors.fill: parent
            visible: input.text === "" && !input.activeFocus
            text: root.placeholder
            font: input.font
            color: Theme.fg3
            verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        id: goBtn
        anchors.right: parent.right
        anchors.rightMargin: 5
        anchors.verticalCenter: parent.verticalCenter
        width: root.large ? 36 : 30
        height: root.large ? 36 : 30
        radius: Theme.radiusMd
        color: Theme.iris

        HoverHandler { id: goHover }
        opacity: goHover.hovered ? 1.12 : 1.0

        VcIcon {
            anchors.centerIn: parent
            width: 15
            height: 15
            name: "arrow-right"
            color: "#FFFFFF"
        }

        TapHandler {
            onTapped: root.submitted(input.text)
        }
    }
}
