import QtQuick
import ViewCam.Studio

// Button in the two mockup flavors:
//   primary — iris pill (Scan for devices)
//   soft    — bg-3 rounded rect (Flip lens, Check for updates)
Rectangle {
    id: root

    property string kind: "primary" // "primary" | "soft"
    property string text: ""
    property string icon: ""
    signal clicked()

    readonly property bool primary: kind === "primary"

    implicitHeight: primary ? 36 : 38
    implicitWidth: content.implicitWidth + (primary ? 36 : 28)
    radius: primary ? Theme.radiusPill : Theme.radiusLg
    color: {
        if (primary)
            return hover.hovered ? Theme.irisHover : Theme.iris
        return hover.hovered ? Theme.bg4 : Theme.bg3
    }
    opacity: 1.0
    border.width: primary ? 0 : 1
    border.color: Theme.line1

    Behavior on color {
        ColorAnimation { duration: Theme.durSnap }
    }

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 8

        VcIcon {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.icon !== ""
            width: root.primary ? 16 : 15
            height: width
            name: root.icon
            color: root.primary ? "#FFFFFF" : (hover.hovered ? Theme.fg1 : Theme.fg2)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.text
            font.family: Theme.fontSans
            font.pixelSize: 13
            font.weight: root.primary ? Font.DemiBold : Font.Medium
            color: root.primary ? "#FFFFFF" : (hover.hovered ? Theme.fg1 : Theme.fg2)
        }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
