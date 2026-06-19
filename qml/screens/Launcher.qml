import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Launcher / pairing overlay: brand bar, pairing guide on the left,
// discovered devices + manual IP on the right, vcam status bar below.
Rectangle {
    id: root

    property bool open: true

    color: Theme.bg1
    opacity: open ? 1 : 0
    visible: opacity > 0
    enabled: true

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.durPull
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.easeIris
        }
    }

    // ── brand bar ────────────────────────────────────────────────
    Rectangle {
        id: bar
        width: parent.width
        height: 56
        color: Theme.bg2

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 22
            spacing: 11

            Monogram {
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 30
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: AppInfo.name
                font.family: Theme.fontSans
                font.pixelSize: 17
                font.weight: Font.DemiBold
                font.letterSpacing: -0.01 * 17
                color: Theme.fg1
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: AppInfo.editionLabel
                font.family: Theme.fontMono
                font.pixelSize: 11
                font.letterSpacing: 0.04 * 11
                color: Theme.fg3
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.line2
        }
    }

    // ── body: pairing guide | divider | device list ──────────────
    Item {
        anchors.top: bar.bottom
        anchors.bottom: statusbar.top
        width: parent.width

        // left column
        Item {
            id: leftCol
            width: (parent.width - 1) / 2
            height: parent.height

            Column {
                anchors.fill: parent
                anchors.margins: 56
                spacing: 0

                Text {
                    text: qsTr("Pair a device").toUpperCase()
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    font.letterSpacing: 0.12 * 11
                    color: Theme.fg3
                }
                Item {
                    width: 1
                    height: 12
                }
                Text {
                    text: qsTr("Connect your phone")
                    font.family: Theme.fontSans
                    font.pixelSize: 30
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.02 * 30
                    color: Theme.fg1
                }
                Item {
                    width: 1
                    height: 12
                }
                Text {
                    width: Math.min(360, parent.width)
                    text: qsTr("%1 turns any phone into a wireless camera and microphone for this computer.").arg(AppInfo.name)
                    font.family: Theme.fontSans
                    font.pixelSize: 15
                    lineHeight: 1.55
                    color: Theme.fg2
                    wrapMode: Text.WordWrap
                }
                Item {
                    width: 1
                    height: 40
                }

                // numbered steps
                Column {
                    width: Math.min(420, parent.width)
                    spacing: 2

                    Repeater {
                        model: [
                            {
                                t: qsTr("Open ViewCam on your phone"),
                                d: qsTr("Launch the mobile app and keep it in the foreground.")
                            },
                            {
                                t: qsTr("Join the same Wi-Fi"),
                                d: qsTr("Both devices need to be on one network.")
                            },
                            {
                                t: qsTr("Pick it from the list →"),
                                d: qsTr("Your phone shows up on the right within a few seconds.")
                            }
                        ]

                        Rectangle {
                            id: step
                            required property int index
                            required property var modelData

                            width: parent.width
                            implicitHeight: stepRow.implicitHeight + 30
                            radius: Theme.radiusLg
                            color: stepHover.hovered ? Theme.bg2 : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: Theme.durSnap
                                }
                            }
                            HoverHandler {
                                id: stepHover
                            }

                            RowLayout {
                                id: stepRow
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                anchors.topMargin: 15
                                anchors.bottomMargin: 15
                                spacing: 16

                                Rectangle {
                                    Layout.preferredWidth: 30
                                    Layout.preferredHeight: 30
                                    Layout.alignment: Qt.AlignTop
                                    radius: 15
                                    color: Theme.irisSoft
                                    border.width: 1
                                    border.color: Theme.alpha(Theme.iris, 0.25)

                                    Text {
                                        anchors.centerIn: parent
                                        text: String(step.index + 1)
                                        font.family: Theme.fontMono
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        color: Theme.iris
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        text: step.modelData.t
                                        font.family: Theme.fontSans
                                        font.pixelSize: 15
                                        font.weight: Font.DemiBold
                                        color: Theme.fg1
                                        wrapMode: Text.WordWrap
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: step.modelData.d
                                        font.family: Theme.fontSans
                                        font.pixelSize: 13
                                        lineHeight: 1.45
                                        color: Theme.fg3
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }

                Item {
                    width: 1
                    height: 36
                }

                Row {
                    visible: AppController.settings.autoDiscovery
                    spacing: 9

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 7
                        height: 7
                        radius: 4
                        color: Theme.blue

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation {
                                to: 1
                                duration: 700
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: Theme.easeIris
                            }
                            NumberAnimation {
                                to: 0.35
                                duration: 700
                                easing.type: Easing.BezierSpline
                                easing.bezierCurve: Theme.easeIris
                            }
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Scanning your network…")
                        font.family: Theme.fontMono
                        font.pixelSize: 13
                        color: Theme.fg3
                    }
                }
            }
        }

        Rectangle {
            anchors.left: leftCol.right
            width: 1
            height: parent.height
            color: Theme.line2
        }

        // right column — available devices + manual connect
        Item {
            anchors.right: parent.right
            width: (parent.width - 1) / 2
            height: parent.height

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 56
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: qsTr("Available devices").toUpperCase()
                        font.family: Theme.fontMono
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        font.letterSpacing: 0.1 * 11
                        color: Theme.fg3
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    Row {
                        visible: AppController.settings.autoDiscovery
                        spacing: 6

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 5
                            height: 5
                            radius: 3
                            color: Theme.blue

                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation {
                                    to: 1
                                    duration: 700
                                }
                                NumberAnimation {
                                    to: 0.35
                                    duration: 700
                                }
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Scanning…")
                            font.family: Theme.fontMono
                            font.pixelSize: 10
                            font.letterSpacing: 0.12 * 10
                            color: Theme.slate
                        }
                    }
                }

                Item {
                    Layout.preferredHeight: 14
                }

                ListView {
                    id: deviceList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: AppController.devices
                    spacing: 10
                    clip: true

                    delegate: VcDeviceCard {
                        required property int index
                        required property string name
                        // model roles fill the card's own host/port properties
                        required host
                        required port

                        width: deviceList.width
                        deviceName: name
                        appearDelay: 150 + index * 50
                        onClicked: AppController.connectToDevice(name, host, port)
                    }
                }

                Item {
                    Layout.preferredHeight: 24
                }

                Text {
                    text: qsTr("Or connect manually")
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                    color: Theme.fg3
                }
                Item {
                    Layout.preferredHeight: 8
                }
                VcIpBox {
                    Layout.fillWidth: true
                    large: true
                    onSubmitted: ip => AppController.connectManual(ip)
                }
            }
        }
    }

    // ── status bar ───────────────────────────────────────────────
    Rectangle {
        id: statusbar
        anchors.bottom: parent.bottom
        width: parent.width
        height: 28
        color: Theme.bg2

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.line2
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 22
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: AppController.virtualCam.available ? Theme.statusLive : Theme.statusError
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: AppController.virtualCam.available ? qsTr("Virtual camera ready · %1").arg(AppController.virtualCam.devicePath) : qsTr("Virtual camera inactive · 0 clients")
                font.family: Theme.fontMono
                font.pixelSize: 11
                color: Theme.fg3
            }
        }
    }
}
// reload trigger Mon Jun 15 01:40:42 AM +03 2026
