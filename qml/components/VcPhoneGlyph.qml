import QtQuick
import ViewCam.Studio

// Stylized phone silhouette: dark slab, notch, faint iris screen glow.
Rectangle {
    id: root

    property bool active: false

    onActiveChanged: screenTint.requestPaint()

    implicitWidth: 36
    implicitHeight: 52
    radius: 6
    color: Theme.bg1
    border.width: 1.5
    border.color: active ? Theme.alpha(Theme.iris, 0.5) : Theme.line3

    // notch
    Rectangle {
        anchors.top: parent.top
        anchors.topMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.max(8, parent.width * 0.32)
        height: 3
        radius: Theme.radiusPill
        color: Theme.line3
    }

    // screen tint — radial iris glow at the upper-left
    Canvas {
        id: screenTint
        anchors.fill: parent
        anchors.topMargin: 9
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        anchors.bottomMargin: 6

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height
            const a = root.active ? 0.30 : 0.25
            const g = ctx.createRadialGradient(w * 0.3, h * 0.2, 0,
                                               w * 0.3, h * 0.2, Math.max(w, h))
            g.addColorStop(0, Qt.rgba(Theme.iris.r, Theme.iris.g, Theme.iris.b, a))
            g.addColorStop(0.7, "transparent")
            ctx.fillStyle = g
            ctx.beginPath()
            ctx.roundedRect(0, 0, w, h, 3, 3)
            ctx.fill()
        }

        Connections {
            target: Theme
            function onIrisChanged() { screenTint.requestPaint() }
        }
    }
}
