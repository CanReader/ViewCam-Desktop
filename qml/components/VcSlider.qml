import QtQuick
import ViewCam.Studio

// HUD volume slider: 96x4 track, iris fill, 34px halo knob with a 22px
// rounded-square iris core (exact mockup construction).
Item {
    id: root

    property real value: 0.6
    signal moved(real value)

    implicitWidth: 96
    implicitHeight: 34

    Rectangle {
        id: track
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 4
        radius: Theme.radiusPill
        color: Theme.bg4
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: track.width * root.value
        height: 4
        radius: Theme.radiusPill
        color: Theme.iris
    }

    Rectangle {
        id: knob
        x: track.width * root.value - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 34
        height: 34
        radius: 17
        color: Theme.alpha(Theme.iris, 0.18)
        border.width: 1
        border.color: Theme.alpha(Theme.iris, 0.4)
        scale: dragArea.pressed ? 1.04 : 1.0

        Rectangle {
            anchors.centerIn: parent
            width: 22
            height: 22
            radius: Theme.radiusMd
            color: Theme.iris
        }
    }

    MouseArea {
        id: dragArea
        anchors.fill: parent
        onPressed: (mouse) => update(mouse.x)
        onPositionChanged: (mouse) => { if (pressed) update(mouse.x) }

        function update(mx) {
            root.value = Math.max(0, Math.min(1, mx / track.width))
            root.moved(root.value)
        }
    }
}
