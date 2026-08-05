import QtQuick
import ViewCam.Studio

// Sources page: Input (phone mic into this computer) and Output (this
// computer's audio out to the phone speaker), as two sub-tabs.
Flickable {
    id: root

    readonly property var s: AppController.settings
    readonly property var audio: AppController.audio
    readonly property bool phoneAudioReady: AppController.connection.connected && audio.phoneAudioCapable
    // 0 = Input, 1 = Output
    property int subTab: 0

    contentHeight: content.y + content.implicitHeight + 128
    clip: true

    Column {
        id: content
        width: Math.min(Theme.contentMax, root.width - 80)
        anchors.horizontalCenter: parent.horizontalCenter
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
            height: 20
        }

        VcSeg {
            model: [qsTr("Input"), qsTr("Output")]
            currentIndex: root.subTab
            onActivated: i => root.subTab = i
        }
        Item {
            width: 1
            height: 20
        }

        // ── Input: the phone microphone into this computer ────────
        Column {
            width: parent.width
            spacing: 0
            visible: root.subTab === 0

            VcCard {
                width: parent.width
                header: qsTr("Inputs")

                VcSettingRow {
                    showDivider: false
                    icon: "mic"
                    accent: true
                    title: AppController.connection.connected ? AppController.connection.deviceName + qsTr(" — Microphone") : qsTr("Phone microphone")
                    description: {
                        if (!AppController.connection.connected)
                            return qsTr("Connect a phone to stream its microphone");
                        if (!root.audio.phoneAudioCapable)
                            return qsTr("Update the phone app to stream audio");
                        if (!root.audio.micEnabled)
                            return qsTr("Muted · PCM 48 kHz");
                        if (!root.audio.micPermission)
                            return qsTr("Allow microphone access on the phone");
                        if (root.audio.micActive && root.audio.micCodec !== "")
                            return qsTr("Live · %1 · %2 · ~%3 ms")
                                .arg(root.audio.micCodec)
                                .arg(root.audio.micTransport)
                                .arg(root.audio.micLatencyMs);
                        return root.audio.micActive ? qsTr("Live · 48 kHz")
                                                    : qsTr("Waiting for phone audio…");
                    }
                    Row {
                        spacing: 14
                        VcMeter {
                            anchors.verticalCenter: parent.verticalCenter
                            // Real level while mic audio flows; the decorative
                            // animation otherwise (a dead-flat meter reads as
                            // broken, and with no signal there IS no level).
                            level: root.audio.micActive ? root.audio.micLevel : -1
                            running: AppController.connection.connected
                        }
                        VcToggle {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: root.audio.micEnabled
                            onToggled: c => root.audio.micEnabled = c
                        }
                    }
                }
                VcSettingRow {
                    icon: "volume"
                    title: qsTr("Input volume")
                    description: qsTr("Software gain on the phone microphone")
                    Row {
                        spacing: 12
                        VcSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            value: root.s.micVolume / 200
                            onMoved: v => root.s.micVolume = Math.round(v * 200)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.s.micVolume + "%"
                            font.family: Theme.fontMono
                            font.pixelSize: 12
                            color: Theme.fg2
                        }
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
                    id: loopbackRow
                    icon: "loopback"
                    title: qsTr("Loopback device")
                    // Windows can't create a mic device in user space — a
                    // cable driver (VB-CABLE et al.) provides it. When it's
                    // missing, say so and link the fix instead of a dead "—".
                    readonly property bool needsDriver:
                        !root.audio.virtualMicReady &&
                        Qt.platform.os === "windows" &&
                        AppController.connection.connected &&
                        root.audio.phoneAudioCapable &&
                        root.audio.micEnabled
                    description: {
                        if (root.audio.virtualMicReady)
                            return qsTr("Select it as the microphone in any app");
                        if (needsDriver)
                            return qsTr("Needs the free VB-CABLE driver — click to download, install, then toggle the mic");
                        return qsTr("Appears while the phone microphone is live");
                    }
                    Text {
                        text: {
                            if (root.audio.virtualMicReady)
                                return root.audio.micSinkDevice;
                            return loopbackRow.needsDriver ? qsTr("Get VB-CABLE") : "—";
                        }
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: loopbackRow.needsDriver ? Theme.iris : Theme.fg2
                        MouseArea {
                            anchors.fill: parent
                            anchors.margins: -6
                            enabled: loopbackRow.needsDriver
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: Qt.openUrlExternally("https://vb-audio.com/Cable/")
                        }
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

        // ── Output: this computer's audio out to the phone speaker ─
        Column {
            width: parent.width
            spacing: 0
            visible: root.subTab === 1

            VcCard {
                width: parent.width
                header: qsTr("Phone speaker")

                VcSettingRow {
                    showDivider: false
                    icon: "wave"
                    accent: true
                    title: qsTr("Capture system audio")
                    description: {
                        if (root.s.captureSystemAudio && root.phoneAudioReady && !root.audio.speakerCaptureRunning)
                            return qsTr("No output device found to capture on this computer");
                        return root.audio.speakerActive ? qsTr("Playing on the phone — wireless speaker") : qsTr("Play what you hear from this computer on the phone");
                    }
                    VcToggle {
                        checked: root.s.captureSystemAudio
                        onToggled: c => root.s.captureSystemAudio = c
                    }
                }
                VcSettingRow {
                    icon: "volume-waves"
                    title: qsTr("Also play on this computer")
                    description: root.s.localPlayback ? qsTr("This computer's speakers stay on while streaming") : qsTr("Phone only — this computer goes silent while streaming")
                    VcToggle {
                        checked: root.s.localPlayback
                        onToggled: c => root.s.localPlayback = c
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
                        text: root.phoneAudioReady && root.audio.speakerActive
                              ? AppController.connection.deviceName : qsTr("System default")
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        color: Theme.fg2
                    }
                }
                VcSettingRow {
                    icon: "volume"
                    title: qsTr("Speaker volume")
                    description: qsTr("Level of the feed sent to the phone")
                    Row {
                        spacing: 12
                        VcSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            value: root.s.speakerVolume / 200
                            onMoved: v => root.s.speakerVolume = Math.round(v * 200)
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.s.speakerVolume + "%"
                            font.family: Theme.fontMono
                            font.pixelSize: 12
                            color: Theme.fg2
                        }
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
        }
    }
}
