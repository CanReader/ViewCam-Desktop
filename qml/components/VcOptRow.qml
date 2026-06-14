import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Right-panel option row (.opt): denser than VcSettingRow, no icon box.
Item {
    id: root

    property string title: ""
    property string description: ""
    property bool showDivider: true
    default property alias rightSlot: right.data

    width: parent ? parent.width : 0
    implicitHeight: layout.implicitHeight + 26

    Rectangle {
        visible: root.showDivider
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.line1
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                font.family: Theme.fontSans
                font.pixelSize: 14
                font.weight: Font.Medium
                color: Theme.fg1
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                visible: root.description !== ""
                text: root.description
                font.family: Theme.fontSans
                font.pixelSize: 12
                color: Theme.fg3
                elide: Text.ElideRight
            }
        }

        Row {
            id: right
            spacing: 8
        }
    }
}
