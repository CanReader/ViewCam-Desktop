import QtQuick
import ViewCam.Studio

// Circular HUD icon button (.iconbtn). `off` shows the muted/error state.
Rectangle {
    id: root

    property string icon: ""
    property bool off: false
    signal clicked()

    implicitWidth: 38
    implicitHeight: 38
    radius: width / 2
    // overlay tracks fg so it stays visible on both the dark and light HUD glass
    color: hover.hovered ? Theme.alpha(Theme.fg1, 0.09) : Theme.alpha(Theme.fg1, 0.05)
    border.width: 1
    border.color: Theme.line2

    Behavior on color {
        ColorAnimation { duration: Theme.durSnap }
    }

    VcIcon {
        anchors.centerIn: parent
        width: 18
        height: 18
        name: root.icon
        color: root.off ? Theme.statusError : (hover.hovered ? Theme.fg1 : Theme.fg2)
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
