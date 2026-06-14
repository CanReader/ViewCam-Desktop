import QtQuick
import ViewCam.Studio

// Info glyph + design-system hover tooltip. Opaque dark surface (bg-3), inset
// hairline, small radius, mono-caption text, a subtle lift, and an iris-eased
// fade-in. Placed ABOVE the glyph so it renders over earlier content rather
// than being hidden behind later rows, and stays inside the (clipped) card.
Item {
    id: root

    property string text: ""
    property int bubbleWidth: 264

    implicitWidth: 15
    implicitHeight: 15

    VcIcon {
        id: glyph
        anchors.fill: parent
        name: "info"
        color: tipHover.hovered ? Theme.fg2 : Theme.fg4
    }
    HoverHandler { id: tipHover }

    // soft lift (kept subtle so the flat-charcoal look survives)
    Rectangle {
        id: shadow
        z: 9
        visible: bubble.opacity > 0
        opacity: bubble.opacity * 0.5
        anchors.fill: bubble
        anchors.margins: -2
        anchors.topMargin: 2
        anchors.bottomMargin: -4
        radius: bubble.radius + 2
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    Rectangle {
        id: bubble
        z: 10
        visible: opacity > 0
        opacity: tipHover.hovered ? 1 : 0

        width: root.bubbleWidth
        implicitHeight: tipText.implicitHeight + 20
        height: implicitHeight

        // anchor above the glyph, left edges aligned
        anchors.bottom: glyph.top
        anchors.bottomMargin: 9
        anchors.left: glyph.left
        anchors.leftMargin: -6

        radius: Theme.radiusMd
        color: Theme.bg3
        border.width: 1
        border.color: Theme.line2

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.durSnap
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeIris
            }
        }

        // 1px top highlight for depth (matches --vc-shadow-hairline-top)
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            anchors.margins: 1
            height: 1
            color: Qt.rgba(1, 1, 1, 0.04)
        }

        Text {
            id: tipText
            anchors.fill: parent
            anchors.margins: 10
            text: root.text
            wrapMode: Text.WordWrap
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            lineHeight: 1.45
            color: Theme.fg2
        }
    }
}
