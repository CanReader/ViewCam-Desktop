import QtQuick
import ViewCam.Studio

// The V-aperture brand mark (inline SVG from the design system).
Item {
    implicitWidth: 30
    implicitHeight: 30

    Image {
        anchors.fill: parent
        source: Theme.assetBase + "images/Logo.svg"
        sourceSize: Qt.size(Math.max(1, Math.round(parent.width * 2)),
                            Math.max(1, Math.round(parent.height * 2)))
        fillMode: Image.PreserveAspectFit
    }
}
