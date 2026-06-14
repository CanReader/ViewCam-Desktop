import QtQuick
import ViewCam.Studio

// Glass telemetry badge on the viewfinder (4K · 60 FPS · 12 Mbps).
// `good` tints the value mint for healthy metrics.
Rectangle {
    id: root

    property string text
    property bool good: false

    implicitHeight: 28
    implicitWidth: label.implicitWidth + 22
    radius: Theme.radiusMd
    color: Theme.glassBg
    border.width: 1
    border.color: root.good ? Theme.alpha(Theme.statusLive, 0.30) : Theme.glassBorder

    Text {
        id: label
        anchors.centerIn: parent
        text: root.text
        font.family: Theme.fontMono
        font.pixelSize: 12
        font.letterSpacing: 0.03 * 11.5
        color: root.good ? Theme.statusLive : Theme.fg2
    }
}
