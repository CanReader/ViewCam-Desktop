import QtQuick
import ViewCam.Studio

// Segmented control: pill track on bg-1, active segment steps to bg-4.
// Labels are mono per the design (numbers/options carry signal).
Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitWidth: row.implicitWidth + 6
    implicitHeight: row.implicitHeight + 6
    radius: Theme.radiusPill
    color: Theme.bg1
    border.width: 1
    border.color: Theme.line1

    Row {
        id: row
        x: 3
        y: 3
        spacing: 2

        Repeater {
            model: root.model

            Rectangle {
                id: seg
                required property int index
                required property var modelData

                implicitWidth: label.implicitWidth + 22
                implicitHeight: label.implicitHeight + 10
                radius: Theme.radiusPill
                color: seg.index === root.currentIndex ? Theme.bg4 : "transparent"

                Behavior on color {
                    ColorAnimation { duration: Theme.durSnap }
                }

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: seg.modelData
                    font.family: Theme.fontMono
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: seg.index === root.currentIndex ? Theme.fg1 : Theme.fg3
                }

                // Owner updates `currentIndex` in response (keeps bindings alive)
                TapHandler {
                    onTapped: root.activated(seg.index)
                }
            }
        }
    }
}
