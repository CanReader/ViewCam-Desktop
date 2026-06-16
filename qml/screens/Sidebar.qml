import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Left sidebar: brand, nav, device deck, profile footer. Fixed 280px.
Rectangle {
    id: root

    property string page: "liveview"
    signal navigate(string page)

    color: Theme.bg2

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.line1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── brand ────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            Layout.topMargin: 22
            Layout.bottomMargin: 18
            spacing: 11

            Monogram { width: 30; height: 30 }

            ColumnLayout {
                spacing: 4

                Text {
                    text: AppInfo.name
                    font.family: Theme.fontSans
                    font.pixelSize: 17
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.01 * 17
                    color: Theme.fg1
                }
                Text {
                    text: AppInfo.editionLabel
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    font.letterSpacing: 0.04 * 11
                    color: Theme.fg3
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── nav ──────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.topMargin: 6
            Layout.bottomMargin: 14
            spacing: 2

            VcNavItem {
                Layout.fillWidth: true
                icon: "camera"; text: qsTr("Live View")
                active: root.page === "liveview"
                onClicked: root.navigate("liveview")
            }
            VcNavItem {
                Layout.fillWidth: true
                icon: "mic-stand"; text: qsTr("Sources")
                active: root.page === "sources"
                onClicked: root.navigate("sources")
            }
            VcNavItem {
                Layout.fillWidth: true
                icon: "gear"; text: qsTr("Settings")
                active: root.page === "settings"
                onClicked: root.navigate("settings")
            }
        }

        // ── device deck (scrollable) ─────────────────────────────
        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            contentHeight: deck.implicitHeight
            clip: true

            ColumnLayout {
                id: deck
                width: parent.width
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    Layout.topMargin: 12
                    Layout.bottomMargin: 10

                    Text {
                        text: qsTr("Devices").toUpperCase()
                        font.family: Theme.fontMono
                        font.pixelSize: 10
                        font.letterSpacing: 0.12 * 10
                        color: Theme.fg3
                    }
                    Item { Layout.fillWidth: true }
                    Row {
                        spacing: 6
                        visible: AppController.settings.autoDiscovery

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 5; height: 5; radius: 3
                            color: Theme.blue

                            SequentialAnimation on opacity {
                                loops: Animation.Infinite
                                NumberAnimation {
                                    to: 1; duration: 700
                                    easing.type: Easing.BezierSpline
                                    easing.bezierCurve: Theme.easeIris
                                }
                                NumberAnimation {
                                    to: 0.35; duration: 700
                                    easing.type: Easing.BezierSpline
                                    easing.bezierCurve: Theme.easeIris
                                }
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Scanning")
                            font.family: Theme.fontMono
                            font.pixelSize: 10
                            font.letterSpacing: 0.12 * 10
                            color: Theme.slate
                        }
                    }
                }

                // connected phone card
                Rectangle {
                    Layout.fillWidth: true
                    visible: AppController.connection.connected
                    implicitHeight: 80
                    radius: Theme.radiusXl
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.alpha(Theme.iris, 0.10) }
                        GradientStop { position: 1; color: Theme.alpha(Theme.iris, 0.03) }
                    }
                    border.width: 1
                    border.color: Theme.alpha(Theme.iris, 0.30)

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 13
                        spacing: 13

                        VcPhoneGlyph {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 54
                            radius: 7
                            active: true
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 7

                                // pulse dot with expanding ring
                                Item {
                                    Layout.preferredWidth: 8
                                    Layout.preferredHeight: 8

                                    Rectangle {
                                        anchors.centerIn: parent
                                        width: 8; height: 8; radius: 4
                                        color: Theme.statusLive
                                    }
                                    Rectangle {
                                        id: ring
                                        anchors.centerIn: parent
                                        width: 16; height: 16; radius: 8
                                        color: "transparent"
                                        border.width: 1.5
                                        border.color: Theme.statusLive
                                        opacity: 0

                                        ParallelAnimation {
                                            running: AppController.connection.connected
                                            loops: Animation.Infinite
                                            NumberAnimation {
                                                target: ring; property: "scale"
                                                from: 0.6; to: 1.6; duration: 1800
                                                easing.type: Easing.BezierSpline
                                                easing.bezierCurve: Theme.easeIris
                                            }
                                            NumberAnimation {
                                                target: ring; property: "opacity"
                                                from: 0.8; to: 0; duration: 1800
                                            }
                                        }
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: AppController.connection.deviceName
                                    font.family: Theme.fontSans
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    color: Theme.fg1
                                    elide: Text.ElideRight
                                }

                                Rectangle {
                                    implicitWidth: liveTag.implicitWidth + 14
                                    implicitHeight: liveTag.implicitHeight + 6
                                    radius: Theme.radiusPill
                                    color: Theme.alpha(Theme.statusLive, 0.10)
                                    border.width: 1
                                    border.color: Theme.alpha(Theme.statusLive, 0.25)

                                    Text {
                                        id: liveTag
                                        anchors.centerIn: parent
                                        text: "LIVE"
                                        font.family: Theme.fontMono
                                        font.pixelSize: 10
                                        font.letterSpacing: 0.08 * 9.5
                                        color: Theme.statusLive
                                    }
                                }
                            }

                            // battery % + connection quality (Strong/Fine/Low/
                            // Terrible) — driven from HELLO metadata + link pacing.
                            Row {
                                id: metaRow
                                spacing: 14

                                readonly property var conn: AppController.connection
                                readonly property color levelColor:
                                    conn.linkQuality >= 2 ? Theme.statusLive
                                    : conn.linkQuality === 1 ? Theme.statusWarn
                                    : Theme.statusError

                                Row {
                                    spacing: 5
                                    VcIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 13; height: 13
                                        name: "battery-full"
                                        color: Theme.fg2
                                        opacity: 0.85
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: metaRow.conn.battery >= 0
                                              ? metaRow.conn.battery + "%" : "—"
                                        font.family: Theme.fontMono
                                        font.pixelSize: 12
                                        color: Theme.fg2
                                    }
                                }

                                Row {
                                    spacing: 5
                                    VcIcon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 13; height: 13
                                        name: "wifi"
                                        color: metaRow.levelColor
                                        opacity: 0.9
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: metaRow.conn.linkQualityLabel
                                        font.family: Theme.fontSans
                                        font.pixelSize: 12
                                        color: Theme.fg2
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.leftMargin: 4
                    Layout.topMargin: 18
                    Layout.bottomMargin: 8
                    text: (AppController.connection.connected ? qsTr("Other devices") : qsTr("Available devices")).toUpperCase()
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    font.letterSpacing: 0.12 * 10
                    color: Theme.fg3
                }

                Repeater {
                    model: AppController.devices

                    VcDeviceRow {
                        required property string name
                        required property int port
                        required host

                        Layout.fillWidth: true
                        // hide the row for the currently connected device
                        readonly property bool isConnected:
                            AppController.connection.connected && host === AppController.connection.host
                        visible: !isConnected
                        Layout.preferredHeight: visible ? implicitHeight : 0

                        deviceName: name
                        onConnectClicked: AppController.connectToDevice(name, host, port)
                    }
                }

                Text {
                    Layout.leftMargin: 4
                    Layout.topMargin: 16
                    Layout.bottomMargin: 8
                    text: qsTr("Connect manually")
                    font.family: Theme.fontSans
                    font.pixelSize: 11
                    color: Theme.fg3
                }
                VcIpBox {
                    Layout.fillWidth: true
                    Layout.leftMargin: 4
                    Layout.rightMargin: 4
                    Layout.bottomMargin: 6
                    onSubmitted: (ip) => AppController.connectManual(ip)
                }
            }
        }

        // ── profile footer ───────────────────────────────────────
        Rectangle {
            visible: false
            Layout.fillWidth: true
            implicitHeight: 60
            color: "transparent"

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: Theme.line1
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 11

                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 16
                    border.width: 1
                    border.color: Theme.line2
                    gradient: Gradient {
                        GradientStop { position: 0; color: Theme.iris }
                        GradientStop { position: 1; color: Theme.blue }
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: qsTr("Operator")
                        font.family: Theme.fontSans
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        color: Theme.fg1
                    }
                    Text {
                        text: qsTr("Free plan")
                        font.family: Theme.fontSans
                        font.pixelSize: 11
                        color: Theme.fg3
                    }
                }

                VcIcon {
                    width: 18; height: 18
                    name: "chevron-down"
                    color: Theme.fg3
                }
            }
        }
    }
}
