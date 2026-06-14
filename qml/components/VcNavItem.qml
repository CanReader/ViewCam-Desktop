import QtQuick
import ViewCam.Studio

// Sidebar nav entry: uppercase 13px label + 18px icon. Active state uses
// iris-soft fill with a faint iris inset hairline.
Rectangle {
    id: root

    property string icon: ""
    property string text: ""
    property bool active: false
    signal clicked()

    implicitHeight: 40
    radius: Theme.radiusLg
    color: active ? Theme.irisSoft : (hover.hovered ? Theme.bg3 : "transparent")
    border.width: active ? 1 : 0
    border.color: Theme.alpha(Theme.iris, 0.18)

    Behavior on color { ColorAnimation { duration: Theme.durSnap } }

    readonly property color tone: active ? Theme.iris
                                         : (hover.hovered ? Theme.fg2 : Theme.slate)

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 14
        spacing: 12

        VcIcon {
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            name: root.icon
            color: root.tone
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text.toUpperCase()
            font.family: Theme.fontSans
            font.pixelSize: 13
            font.weight: Font.Medium
            font.letterSpacing: 0.06 * 13
            color: root.tone
        }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
