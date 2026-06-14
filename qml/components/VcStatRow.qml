import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Right-panel stat line (.stat): icon + key on the left, mono value on the
// right. `good` tints the value mint. Custom right content (signal bars)
// goes in the default slot and replaces the value text.
Item {
    id: root

    property string icon: ""
    property string label: ""
    property string value: ""
    property bool good: false
    property bool showDivider: true
    default property alias rightSlot: right.data

    width: parent ? parent.width : 0
    implicitHeight: 35

    Rectangle {
        visible: root.showDivider
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.line1
    }

    RowLayout {
        anchors.fill: parent
        spacing: 12

        Row {
            spacing: 10

            VcIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.icon !== ""
                width: 15
                height: 15
                name: root.icon
                color: Theme.fg3
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.label
                font.family: Theme.fontSans
                font.pixelSize: 13
                color: Theme.fg2
            }
        }

        Item { Layout.fillWidth: true }

        Row {
            id: right
            spacing: 0

            Text {
                visible: root.value !== ""
                text: root.value
                font.family: Theme.fontMono
                font.pixelSize: 13
                color: root.good ? Theme.statusLive : Theme.fg1
            }
        }
    }
}
