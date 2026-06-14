import QtQuick
import ViewCam.Studio

// 44x26 switch. On = iris fill, knob slides 18px with the snap ease.
Rectangle {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 44
    implicitHeight: 26
    radius: Theme.radiusPill
    color: checked ? Theme.iris : Theme.bg4
    border.width: checked ? 0 : 1
    border.color: Theme.line2

    Behavior on color {
        ColorAnimation { duration: Theme.durSnap }
    }

    Rectangle {
        x: root.checked ? 21 : 3
        y: 3
        width: 20
        height: 20
        radius: 10
        color: root.checked ? "#FFFFFF" : Theme.fg2

        Behavior on x {
            NumberAnimation {
                duration: Theme.durSnap
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeSnap
            }
        }
        Behavior on color {
            ColorAnimation { duration: Theme.durSnap }
        }
    }

    // Owner updates `checked` (usually via binding to a ViewModel property);
    // the toggle itself never mutates its own state.
    TapHandler {
        onTapped: root.toggled(!root.checked)
    }
}
