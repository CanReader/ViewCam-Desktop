import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import ViewCam.Studio

// Right stats panel: network + device stats, device control, optimization,
// disconnect footer. Fixed 344px, visible on Live View only.
Rectangle {
    id: root

    readonly property var conn: AppController.connection
    readonly property var s: AppController.settings
    // camera controls — backed by the phone over the CONTROL channel
    readonly property var cc: AppController.cameraControl
    readonly property bool isPro: s.isPro   // Debug unlocks all; release gated until IAP lands

    property bool optimizationBlurred: true

    VcProGateDialog { id: proGate }

    color: Theme.bg2

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.line1
    }

    Flickable {
        anchors.top: parent.top
        anchors.bottom: footer.top
        width: parent.width
        contentHeight: panel.implicitHeight + 40
        clip: true

        Column {
            id: panel
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 22

            // ── Network ─────────────────────────────────────────
            Column {
                width: parent.width
                spacing: 0

                Text {
                    text: qsTr("Network").toUpperCase()
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    font.letterSpacing: 0.12 * 10
                    color: Theme.fg3
                }
                Item {
                    width: 1
                    height: 12
                }

                VcStatRow {
                    showDivider: false
                    icon: "wifi"
                    label: qsTr("Connection")
                    value: root.conn.connected ? "TCP · MJPEG" : "—"
                }
                VcStatRow {
                    icon: "address"
                    label: qsTr("Address")
                    value: root.conn.connected ? root.conn.host + " : " + root.conn.port : "—"
                }
                VcStatRow {
                    icon: "signal-graph"
                    label: qsTr("Signal")

                    // bars lit = link quality (Terrible..Strong); colour shifts
                    // mint → amber → red as the link degrades. Each bar reads the
                    // connection directly (root.conn) so it resolves inside the slot.
                    Row {
                        spacing: 2
                        height: 13

                        Repeater {
                            model: 4
                            Rectangle {
                                required property int index
                                readonly property var heights: [0.35, 0.6, 0.82, 1.0]
                                readonly property int q: root.conn.connected ? root.conn.linkQuality : -1
                                anchors.bottom: parent.bottom
                                width: 3
                                height: 13 * heights[index]
                                radius: 1
                                color: index > q ? Theme.fg4 : q >= 2 ? Theme.statusLive : q === 1 ? Theme.statusWarn : Theme.statusError
                            }
                        }
                    }
                }
                VcStatRow {
                    icon: "latency"
                    label: qsTr("Latency")
                    value: root.conn.connected && root.conn.frameIntervalMs > 0 ? root.conn.frameIntervalMs.toFixed(0) + " ms" : "—"
                    good: root.conn.connected && root.conn.frameIntervalMs > 0 && root.conn.frameIntervalMs < 34
                }
                VcStatRow {
                    icon: "bitrate"
                    label: qsTr("Bitrate")
                    // 0 here means "connected but no stream yet" — show "—", not 0.0
                    value: root.conn.connected && root.conn.bitrateMbps > 0 ? root.conn.bitrateMbps.toFixed(1) + " Mbps" : "—"
                }
                VcStatRow {
                    icon: "packet"
                    label: qsTr("FPS")
                    value: root.conn.connected && root.conn.fps > 0 ? root.conn.fps + " fps" : "—"
                    good: root.conn.connected && root.conn.fps >= 24
                }
                VcStatRow {
                    icon: "clock"
                    label: qsTr("Uptime")
                    value: root.conn.uptimeText
                }
            }

            // ── Device ──────────────────────────────────────────
            Column {
                width: parent.width
                spacing: 0

                Text {
                    text: qsTr("Device").toUpperCase()
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    font.letterSpacing: 0.12 * 10
                    color: Theme.fg3
                }
                Item {
                    width: 1
                    height: 12
                }

                VcStatRow {
                    showDivider: false
                    icon: "phone"
                    label: qsTr("Model")
                    value: root.conn.connected ? root.conn.deviceName : "—"
                }
                VcStatRow {
                    icon: "system"
                    label: qsTr("System")
                    value: (root.conn.connected && root.conn.deviceOs !== "") ? root.conn.deviceOs : "—"
                }
                VcStatRow {
                    icon: "battery-full"
                    label: qsTr("Battery")
                    // never render the raw -1 sentinel → em dash when unknown
                    value: !(root.conn.connected && root.conn.battery >= 0) ? "—" : root.conn.charging ? root.conn.battery + "% · " + qsTr("charging") : root.conn.battery + "%"
                    good: root.conn.connected && root.conn.charging
                }
                VcStatRow {
                    icon: "camera"
                    label: qsTr("Lens")
                    value: root.conn.connected && root.conn.lens !== "" ? root.conn.lens : "—"
                }
                VcStatRow {
                    icon: "capture"
                    label: qsTr("Capture")
                    value: root.conn.connected && root.conn.frameWidth > 0 ? root.conn.frameWidth + "×" + root.conn.frameHeight + " · " + root.conn.fps + " fps" : "—"
                }
            }

            // ── Device control ──────────────────────────────────
            Column {
                width: parent.width
                spacing: 0

                Text {
                    text: qsTr("Device control").toUpperCase()
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    font.letterSpacing: 0.12 * 10
                    color: Theme.fg3
                }
                Item {
                    width: 1
                    height: 12
                }

                VcOptRow {
                    // disabled + dimmed when the active camera has no flash unit
                    // (e.g. front camera); updates live as the lens changes.
                    showDivider: false
                    title: qsTr("Torch")
                    description: root.cc.torchAvailable ? qsTr("Rear flashlight") : qsTr("No flash on front camera")
                    opacity: root.cc.torchAvailable ? 1.0 : 0.45
                    VcToggle {
                        checked: root.cc.torch
                        enabled: root.conn.connected && root.cc.torchAvailable
                        onToggled: c => root.cc.setTorch(c)
                    }
                }
                VcOptRow {
                    title: qsTr("Focus lock")
                    description: qsTr("Freeze autofocus")
                    VcToggle {
                        checked: root.cc.focusLock
                        enabled: root.conn.connected
                        onToggled: c => root.cc.setFocusLock(c)
                    }
                }
                VcOptRow {
                    title: qsTr("Exposure lock")
                    description: qsTr("Lock AE / AWB")
                    VcToggle {
                        checked: root.cc.exposureLock
                        enabled: root.conn.connected
                        onToggled: c => root.cc.setExposureLock(c)
                    }
                }
                VcOptRow {
                    // disabled + dimmed when the phone reports HDR unsupported
                    title: qsTr("HDR")
                    description: root.cc.hdrSupported ? qsTr("High dynamic range") : qsTr("Not supported on this device")
                    opacity: root.cc.hdrSupported ? 1.0 : 0.45
                    pro: true
                    isPro: root.isPro
                    onProGateTapped: proGate.open()
                    VcToggle {
                        checked: root.cc.hdr
                        enabled: root.conn.connected && root.cc.hdrSupported
                        onToggled: c => root.cc.setHdr(c)
                    }
                }
                VcOptRow {
                    title: qsTr("Resolution")
                    // 1080p (index 2) is a Pro feature; free users get 480p/720p
                    // and tapping 1080p opens the upgrade gate instead of applying.
                    VcSeg {
                        model: ["480p", "720p", "1080p"]
                        currentIndex: root.cc.resolutionIndex
                        onActivated: i => {
                            if (i === 2 && !root.isPro)
                                proGate.open()
                            else
                                root.cc.setResolution(i)
                        }
                    }
                }

                Item {
                    width: 1
                    height: 12
                }
                RowLayout {
                    width: parent.width
                    spacing: 8

                    VcButton {
                        Layout.fillWidth: true
                        kind: "soft"
                        icon: "flip"
                        text: qsTr("Flip lens")
                        onClicked: root.cc.flipLens()
                    }
                    VcButton {
                        Layout.fillWidth: true
                        kind: "soft"
                        icon: "focus"
                        text: qsTr("Tap to focus")
                        onClicked: root.cc.triggerFocus()
                    }
                }
            }

            // ── Optimization ────────────────────────────────────
            Item {
                id: optimizationSection
                width: parent.width
                height: optimizationContent.implicitHeight

                Column {
                    id: optimizationContent
                    width: parent.width
                    spacing: 0
                    layer.enabled: root.optimizationBlurred
                    opacity: root.optimizationBlurred ? 0 : 1

                    Text {
                        text: qsTr("Optimization").toUpperCase()
                        font.family: Theme.fontMono
                        font.pixelSize: 10
                        font.letterSpacing: 0.12 * 10
                        color: Theme.fg3
                    }
                    Item {
                        width: 1
                        height: 12
                    }

                    VcOptRow {
                        showDivider: false
                        title: qsTr("Adaptive bitrate")
                        description: qsTr("Clamps to network")
                        VcToggle {
                            checked: root.s.adaptiveBitrate
                            onToggled: c => root.s.adaptiveBitrate = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Hardware acceleration")
                        description: qsTr("GPU encode & decode")
                        VcToggle {
                            checked: root.s.hardwareAccel
                            onToggled: c => root.s.hardwareAccel = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Adaptive resolution")
                        description: qsTr("Drop res before frames")
                        VcToggle {
                            checked: root.s.adaptiveResolution
                            onToggled: c => root.s.adaptiveResolution = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Low-light boost")
                        description: qsTr("Brighten dim scenes")
                        VcToggle {
                            checked: root.s.lowLightBoost
                            onToggled: c => root.s.lowLightBoost = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Noise reduction")
                        description: qsTr("Temporal denoise")
                        VcToggle {
                            checked: root.s.noiseReduction
                            onToggled: c => root.s.noiseReduction = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Mirror image")
                        description: qsTr("Flip horizontally")
                        VcToggle {
                            checked: root.s.mirrorImage
                            onToggled: c => root.s.mirrorImage = c
                        }
                    }
                    VcOptRow {
                        title: qsTr("Color profile")
                        VcSeg {
                            model: [qsTr("Auto"), "sRGB", "P3"]
                            currentIndex: root.s.colorProfile
                            onActivated: i => root.s.colorProfile = i
                        }
                    }
                    VcOptRow {
                        title: qsTr("Audio sample rate")
                        VcSeg {
                            model: ["44.1k", "48k", "96k"]
                            currentIndex: root.s.audioSampleRate
                            onActivated: i => root.s.audioSampleRate = i
                        }
                    }
                }

                MultiEffect {
                    anchors.fill: optimizationContent
                    source: optimizationContent
                    blurEnabled: true
                    blurMax: 48
                    blur: 1.0
                    brightness: -0.02
                    visible: root.optimizationBlurred
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: root.optimizationBlurred
                    visible: root.optimizationBlurred
                }

                Rectangle {
                    anchors.centerIn: parent
                    visible: root.optimizationBlurred
                    color: Qt.rgba(0.05, 0.07, 0.12, 0.05)
                    width: optUnavailableText.width + 20
                    height: optUnavailableText.height + 20
                    radius: 15

                    Text {
                        id: optUnavailableText
                        anchors.centerIn: parent
                        text: qsTr("This feature is currently unavailable!\n Please wait for the next update...")
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.fg1
                        font.family: Theme.fontMono
                        font.pixelSize: 14
                        font.letterSpacing: -0.01 * 60
                    }
                }
            }
        }
    }

    // ── footer: disconnect ───────────────────────────────────────
    Rectangle {
        id: footer
        anchors.bottom: parent.bottom
        width: parent.width
        height: 70
        color: "transparent"

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: Theme.line1
        }

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 16
            anchors.bottomMargin: 14
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            radius: Theme.radiusLg
            color: discHover.hovered ? Theme.signalSoft : "transparent"

            Behavior on color {
                ColorAnimation {
                    duration: Theme.durSnap
                }
            }

            Row {
                anchors.centerIn: parent
                spacing: 8

                VcIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 15
                    height: 15
                    name: "power"
                    color: Theme.statusError
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Disconnect device")
                    font.family: Theme.fontSans
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.statusError
                }
            }

            HoverHandler {
                id: discHover
            }
            TapHandler {
                onTapped: AppController.disconnectDevice()
            }
        }
    }
}
