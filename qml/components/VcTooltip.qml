import QtQuick
import QtQuick.Controls
import ViewCam.Studio

// Info glyph + design-system hover tooltip. Opaque dark surface (bg-3), inset
// hairline, small radius, mono-caption text, a subtle lift, and an iris-eased
// fade-in. The bubble is reparented to the window overlay via Popup so it
// escapes any clip: true ancestor (e.g. VcCard).
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

    Popup {
        id: bubble
        visible: tipHover.hovered

        // position relative to glyph mapped to window coordinates
        x: glyph.mapToItem(null, -6, 0).x
        y: glyph.mapToItem(null, 0, -implicitHeight - 9).y

        width: root.bubbleWidth
        implicitHeight: tipText.implicitHeight + 20

        padding: 0

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.bg3
            border.width: 1
            border.color: Theme.line2

            // 1px top highlight for depth (matches --vc-shadow-hairline-top)
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                anchors.margins: 1
                height: 1
                color: Qt.rgba(1, 1, 1, 0.04)
            }
        }

        contentItem: Text {
            id: tipText
            text: root.text
            wrapMode: Text.WordWrap
            font.family: Theme.fontSans
            font.pixelSize: Theme.textXs
            lineHeight: 1.45
            color: Theme.fg2
            leftPadding: 10
            rightPadding: 10
            topPadding: 10
            bottomPadding: 10
        }

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0; to: 1
                duration: Theme.durSnap
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeIris
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1; to: 0
                duration: Theme.durSnap
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeIris
            }
        }
    }
}
