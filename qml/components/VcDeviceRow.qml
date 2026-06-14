import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Sidebar "Other devices" row (.row): small glyph, name, ip, Connect pill.
Rectangle {
    id: root

    property string deviceName: ""
    property string host: ""
    signal connectClicked()

    implicitHeight: 58
    radius: Theme.radiusLg
    color: hover.hovered ? Theme.bg3 : "transparent"

    Behavior on color { ColorAnimation { duration: Theme.durSnap } }

    HoverHandler { id: hover }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 11

        VcPhoneGlyph {
            Layout.preferredWidth: 26
            Layout.preferredHeight: 38
            radius: 5
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.deviceName
                font.family: Theme.fontSans
                font.pixelSize: 13
                font.weight: Font.Medium
                color: Theme.fg2
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: root.host
                font.family: Theme.fontMono
                font.pixelSize: 11
                color: Theme.fg3
                elide: Text.ElideRight
            }
        }

        Rectangle {
            implicitWidth: connectLabel.implicitWidth + 24
            implicitHeight: 28
            radius: Theme.radiusPill
            color: hover.hovered ? Theme.irisSoft : "transparent"
            border.width: 1
            border.color: hover.hovered ? Theme.alpha(Theme.iris, 0.4) : Theme.line1

            Behavior on color { ColorAnimation { duration: Theme.durSnap } }
            Behavior on border.color { ColorAnimation { duration: Theme.durSnap } }

            Text {
                id: connectLabel
                anchors.centerIn: parent
                text: qsTr("Connect")
                font.family: Theme.fontSans
                font.pixelSize: 12
                font.weight: Font.Medium
                color: hover.hovered ? Theme.iris : Theme.fg3
            }

            TapHandler { onTapped: root.connectClicked() }
        }
    }
}
