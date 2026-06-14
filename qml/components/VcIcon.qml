import QtQuick
import QtQuick.Effects
import ViewCam.Studio

// Stroke icon from the design set, tinted at runtime.
// Source SVGs are white; MultiEffect colorization maps them to `color`.
Item {
    id: root

    property string name
    property color color: Theme.fg2

    implicitWidth: 18
    implicitHeight: 18

    Image {
        id: img
        anchors.fill: parent
        source: root.name ? Theme.icon(root.name) : ""
        sourceSize: Qt.size(Math.max(1, Math.round(root.width * 2)),
                            Math.max(1, Math.round(root.height * 2)))
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: img
        colorization: 1
        colorizationColor: root.color
    }
}
