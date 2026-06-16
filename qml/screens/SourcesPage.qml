import QtQuick
import QtQuick.Effects
import ViewCam.Studio

// Sources page: audio inputs, output & monitoring, processing.
Flickable {
    id: root

    readonly property var s: AppController.settings
    property var isBlurred: true

    contentHeight: content.implicitHeight + 128
    clip: true

    Item {
        id: contentContainer
        width: Math.min(Theme.contentMax, root.width - 80)
        height: content.y + content.implicitHeight
        anchors.horizontalCenter: parent.horizontalCenter
        layer.enabled: root.isBlurred
        opacity: root.isBlurred ? 0 : 1

        Column {
            id: content
            width: parent.width
            y: 48
            spacing: 0

            Text {
                text: qsTr("Sources")
                font.family: Theme.fontSans
                font.pixelSize: 28
                font.weight: Font.DemiBold
                font.letterSpacing: -0.01 * 28
                color: Theme.fg1
            }

            Item {
                width: 1
                height: 8
            }

            Text {
                width: parent.width
                text: qsTr("How this computer captures, mixes and plays back audio. The connected phone is already linked — no re-pairing needed.")
                font.family: Theme.fontSans
                font.pixelSize: 14
                color: Theme.fg2
                wrapMode: Text.WordWrap
            }
            Item {
                width: 1
                height: 24
            }

            VcCard {
                width: parent.width
                header: qsTr("Inputs")

                VcSettingRow {
                    showDivider: false
                    icon: "mic"
                    accent: true
                    title: AppController.connection.connected ? AppController.connection.deviceName + qsTr(" — Microphone") : qsTr("Phone microphone")
                    description: qsTr("From phone · Compressed 64 Kb/s · 48 kHz")
                    VcMeter {
                        running: AppController.connection.connected
                    }
                }
                VcSettingRow {
                    icon: "wave"
                    accent: true
                    title: qsTr("Capture system audio")
                    description: qsTr("Record what you hear from this computer")
                    VcToggle {
                        checked: root.s.captureSystemAudio
                        onToggled: c => root.s.captureSystemAudio = c
                    }
                }
                VcSettingRow {
                    icon: "app-window"
                    title: qsTr("Application capture")
                    description: qsTr("Capture audio from one app only")
                    VcSeg {
                        model: [qsTr("Off"), qsTr("Pick app")]
                        currentIndex: root.s.appCapture
                        onActivated: i => root.s.appCapture = i
                    }
                }
                VcSettingRow {
                    icon: "loopback"
                    title: qsTr("Loopback device")
                    description: qsTr("Virtual cable for routing to other apps")
                    Text {
                        text: "ViewCam Audio"
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: Theme.fg2
                    }
                }
            }
            Item {
                width: 1
                height: 24
            }

            VcCard {
                width: parent.width
                header: qsTr("Output & monitoring")

                VcSettingRow {
                    showDivider: false
                    icon: "volume-waves"
                    accent: true
                    title: qsTr("Output device")
                    description: qsTr("Where the return feed plays")
                    Text {
                        text: qsTr("System default")
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: Theme.fg2
                    }
                }
                VcSettingRow {
                    icon: "volume"
                    title: qsTr("Monitor volume")
                    description: qsTr("Loopback monitoring level")
                    Text {
                        text: "72%"
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: Theme.fg2
                    }
                }
                VcSettingRow {
                    icon: "meters"
                    title: qsTr("Mute monitor while talking")
                    description: qsTr("Avoid echo on calls")
                    VcToggle {
                        checked: root.s.monitorMute
                        onToggled: c => root.s.monitorMute = c
                    }
                }
            }
            Item {
                width: 1
                height: 24
            }

            VcCard {
                width: parent.width
                header: qsTr("Processing")

                VcSettingRow {
                    showDivider: false
                    icon: "wave-mid"
                    accent: true
                    title: qsTr("Sample rate")
                    description: qsTr("Mixer clock")
                    VcSeg {
                        model: ["44.1k", "48k", "96k"]
                        currentIndex: root.s.sampleRate
                        onActivated: i => root.s.sampleRate = i
                    }
                }
                VcSettingRow {
                    icon: "buffer"
                    title: qsTr("Buffer size")
                    description: qsTr("Lower = less delay")
                    VcSeg {
                        model: [qsTr("Low"), qsTr("Medium"), qsTr("High")]
                        currentIndex: root.s.bufferSize
                        onActivated: i => root.s.bufferSize = i
                    }
                }
                VcSettingRow {
                    icon: "plus"
                    title: qsTr("A/V sync offset")
                    description: qsTr("Nudge audio against video")
                    Text {
                        text: "0 ms"
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: Theme.fg2
                    }
                }
                VcSettingRow {
                    icon: "mono-circle"
                    title: qsTr("Channels")
                    description: qsTr("Mono is lighter on the network")
                    VcSeg {
                        model: [qsTr("Mono"), qsTr("Stereo")]
                        currentIndex: root.s.channels
                        onActivated: i => root.s.channels = i
                    }
                }
                VcSettingRow {
                    icon: "gate"
                    title: qsTr("Noise gate")
                    description: qsTr("Silence the feed below a threshold")
                    VcToggle {
                        checked: root.s.noiseGate
                        onToggled: c => root.s.noiseGate = c
                    }
                }
            }
        }
    }

    MultiEffect {
        id: blur
        anchors.fill: contentContainer
        source: contentContainer

        blurEnabled: true
        blurMax: 48
        blur: 1.0
        brightness: -0.02

        visible: root.isBlurred
    }

    MouseArea {
        anchors.fill: contentContainer
        enabled: root.isBlurred
        visible: root.isBlurred
    }

    Rectangle {
        id: unavailableOverlay
        anchors.centerIn: contentContainer
        color: Qt.rgba(0.05, 0.07, 0.12, 0.05)
        width: unavailableText.width + 20
        height: unavailableText.height + 20
        radius: 15
        visible: root.isBlurred

        Text {
            id: unavailableText
            anchors.centerIn: parent
            text: qsTr("This feature is currently unavailable!\n Please wait for the next update...")
            horizontalAlignment: Text.AlignHCenter
            color: Theme.fg1
            font.family: Theme.fontMono
            font.pixelSize: 18
            font.letterSpacing: -0.01 * 60
        }
    }
}
