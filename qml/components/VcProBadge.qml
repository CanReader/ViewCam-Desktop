import QtQuick
import ViewCam.Studio

// Small iris pill badge marking a Pro-only feature.
Rectangle {
    id: root
    implicitWidth: label.implicitWidth + 12
    implicitHeight: 20
    radius: Theme.radiusPill
    color: Theme.iris

    Text {
        id: label
        anchors.centerIn: parent
        text: "PRO"
        font.family: Theme.fontSans
        font.pixelSize: 9
        font.weight: Font.ExtraBold
        font.letterSpacing: 0.08 * 9
        color: "white"
    }
}
