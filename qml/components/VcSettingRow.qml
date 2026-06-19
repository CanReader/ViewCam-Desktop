import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Settings/sources row (.srow): 36px icon box, title + description, right
// slot for a toggle / segmented control / mono value. Hairline on top
// except for the first row in a card.
Item {
    id: root

    property string icon: ""
    property bool accent: false       // iris icon box vs muted
    property string title: ""
    property string description: ""
    property string tooltip: ""       // optional hover help (info glyph)
    property bool showDivider: true
    property bool pro: false
    property bool isPro: false
    signal proGateTapped()
    default property alias rightSlot: right.data

    width: parent ? parent.width : 0
    implicitHeight: layout.implicitHeight + 36

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
        anchors.leftMargin: 22
        anchors.rightMargin: 22
        spacing: 16

        Rectangle {
            visible: root.icon !== ""
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            radius: Theme.radiusMd
            color: root.accent ? Theme.irisSoft : Theme.bg3

            VcIcon {
                anchors.centerIn: parent
                width: 18
                height: 18
                name: root.icon
                color: root.accent ? Theme.iris : Theme.fg3
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            RowLayout {
                Layout.fillWidth: true
                spacing: 7

                Text {
                    text: root.title
                    font.family: Theme.fontSans
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: Theme.fg1
                    elide: Text.ElideRight
                }
                VcProBadge {
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.pro && !root.isPro
                }
                VcTooltip {
                    Layout.alignment: Qt.AlignVCenter
                    visible: root.tooltip !== ""
                    text: root.tooltip
                }
                Item { Layout.fillWidth: true }
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

        // Wrapper so the gate MouseArea can sit on top of controls via z-order.
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

            // Gate overlay — declared after Row so it renders on top;
            // blocks all mouse events to the controls when Pro-locked.
            MouseArea {
                anchors.fill: parent
                visible: root.pro && !root.isPro
                cursorShape: Qt.PointingHandCursor
                onClicked: root.proGateTapped()
            }
        }
    }
}
