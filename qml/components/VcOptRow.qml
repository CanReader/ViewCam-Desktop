import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Right-panel option row (.opt): denser than VcSettingRow, no icon box.
Item {
    id: root

    property string title: ""
    property string description: ""
    property bool showDivider: true
    property bool pro: false
    property bool isPro: false
    signal proGateTapped()
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
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Row {
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                    font.family: Theme.fontSans
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    color: Theme.fg1
                    elide: Text.ElideRight
                }
                VcProBadge {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.pro && !root.isPro
                }
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

        Item {
            id: rightWrapper
            implicitWidth: right.implicitWidth
            implicitHeight: right.implicitHeight
            Layout.alignment: Qt.AlignVCenter

            Row {
                id: right
                anchors.centerIn: parent
                spacing: 8
                opacity: (root.pro && !root.isPro) ? 0.32 : 1.0
            }

            MouseArea {
                anchors.fill: parent
                visible: root.pro && !root.isPro
                cursorShape: Qt.PointingHandCursor
                onClicked: root.proGateTapped()
            }
        }
    }
}
