import QtQuick
import ViewCam.Studio

// Content card: bg-2, 14px radius, inset hairline, mono uppercase header
// with a bottom hairline. Children stack below the header.
Rectangle {
    id: root

    property string header: ""
    default property alias contentData: contentCol.data

    color: Theme.bg2
    radius: Theme.radiusXl
    border.width: 1
    border.color: Theme.line1
    clip: true
    implicitHeight: headerItem.height + contentCol.implicitHeight

    Item {
        id: headerItem
        width: parent.width
        height: root.header !== "" ? 46 : 0
        visible: root.header !== ""

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 22
            text: root.header.toUpperCase()
            font.family: Theme.fontMono
            font.pixelSize: 10
            font.letterSpacing: 0.12 * 10
            color: Theme.fg3
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.line1
        }
    }

    Column {
        id: contentCol
        anchors.top: headerItem.bottom
        width: parent.width
    }
}
