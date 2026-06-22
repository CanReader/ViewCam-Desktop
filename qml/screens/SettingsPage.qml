import QtQuick
import QtQuick.Controls
import ViewCam.Studio

// Settings page: stream engine, network, appearance, about.
Flickable {
    id: root

    readonly property var s: AppController.settings
    readonly property bool isPro: false   // wire to AppController.settings.isPro when IAP lands

    VcProGateDialog { id: proGate }

    contentHeight: content.implicitHeight + 128
    clip: true

    Column {
        id: content
        width: Math.min(Theme.contentMax, root.width - 80)
        anchors.horizontalCenter: parent.horizontalCenter
        y: 48
        spacing: 0

        Text {
            text: qsTr("Settings")
            font.family: Theme.fontSans
            font.pixelSize: 28
            font.weight: Font.DemiBold
            font.letterSpacing: -0.01 * 28
            color: Theme.fg1
        }
        Item { width: 1; height: 8 }
        Text {
            width: parent.width
            text: qsTr("Engine, network and appearance preferences for %1.").arg(AppInfo.displayName)
            font.family: Theme.fontSans
            font.pixelSize: 14
            color: Theme.fg2
            wrapMode: Text.WordWrap
        }
        Item { width: 1; height: 24 }

        VcCard {
            width: parent.width
            header: qsTr("Stream engine")

            VcSettingRow {
                showDivider: false
                icon: "gpu"; accent: true
                title: qsTr("Hardware acceleration")
                description: qsTr("Use GPU encoder (VideoToolbox / NVENC)")
                tooltip: qsTr("GPU video encoding — encodes the virtual-camera stream on the GPU (NVENC / VideoToolbox). Off = CPU encoding.")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                VcToggle {
                    checked: root.s.hardwareAccel
                    onToggled: (c) => root.s.hardwareAccel = c
                }
            }
            VcSettingRow {
                icon: "renderer"
                title: qsTr("GPU processing")
                description: AppController.gpuBackend !== ""
                             ? qsTr("Active: %1").arg(AppController.gpuBackend)
                             : qsTr("Frame processing & filters")
                tooltip: qsTr("GPU-accelerated frame processing & filters — CUDA on NVIDIA, Vulkan compute on AMD/Intel. Off = CPU.")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                VcToggle {
                    checked: root.s.gpuProcessing
                    onToggled: (c) => root.s.gpuProcessing = c
                }
            }
            VcSettingRow {
                icon: "protocol"
                title: qsTr("Stream protocol")
                description: qsTr("MJPEG active · H.264/H.265 require mobile update")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                VcSeg {
                    model: ["MJPEG", "H.264", "H.265"]
                    currentIndex: root.s.streamProtocol
                    onActivated: (i) => root.s.streamProtocol = i
                }
            }
            VcSettingRow {
                icon: "plus"
                title: qsTr("Encoder preset")
                description: qsTr("MJPEG quality / H.264 speed trade-off")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                VcSeg {
                    model: [qsTr("Quality"), qsTr("Balanced"), qsTr("Speed")]
                    currentIndex: root.s.encoderPreset
                    onActivated: (i) => root.s.encoderPreset = i
                }
            }
            VcSettingRow {
                icon: "resolution"
                title: qsTr("Max output resolution")
                description: qsTr("Virtual camera ceiling")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                VcSeg {
                    model: ["720p", "1080p", "4K"]
                    currentIndex: root.s.maxResolution
                    onActivated: (i) => root.s.maxResolution = i
                }
            }
            VcSettingRow {
                icon: "clock"
                title: qsTr("Keyframe interval")
                description: qsTr("I-frame rate for H.264/H.265 — no effect on MJPEG")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                Row {
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    VcButton {
                        kind: "soft"; text: "−"
                        onClicked: if (root.s.keyframeInterval > 1) root.s.keyframeInterval--
                    }
                    Text {
                        width: 36; height: 38
                        text: root.s.keyframeInterval + " s"
                        font.family: Theme.fontMono; font.pixelSize: 12; color: Theme.fg2
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    VcButton {
                        kind: "soft"; text: "+"
                        onClicked: if (root.s.keyframeInterval < 10) root.s.keyframeInterval++
                    }
                }
            }
            VcSettingRow {
                icon: "arch"
                title: qsTr("Buffered frames")
                description: qsTr("Delays display by N frames to smooth network jitter")
                pro: true
                isPro: root.isPro
                onProGateTapped: proGate.open()
                Row {
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    VcButton {
                        kind: "soft"; text: "−"
                        onClicked: if (root.s.bufferedFrames > 0) root.s.bufferedFrames--
                    }
                    Text {
                        width: 64; height: 38
                        text: root.s.bufferedFrames === 0 ? qsTr("Off") : (root.s.bufferedFrames + qsTr(" frames"))
                        font.family: Theme.fontMono; font.pixelSize: 12; color: Theme.fg2
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    VcButton {
                        kind: "soft"; text: "+"
                        onClicked: if (root.s.bufferedFrames < 8) root.s.bufferedFrames++
                    }
                }
            }
        }
        Item { width: 1; height: 24 }

        VcCard {
            width: parent.width
            header: qsTr("Network")

            VcSettingRow {
                showDivider: false
                icon: "wifi"; accent: true
                title: qsTr("Automatic discovery")
                description: qsTr("Announce this computer to nearby phones")
                VcToggle {
                    checked: root.s.autoDiscovery
                    onToggled: (c) => root.s.autoDiscovery = c
                }
            }
            VcSettingRow {
                icon: "port"
                title: qsTr("Listen port")
                description: qsTr("TCP stream socket — applies on next connect")
                TextInput {
                    id: portInput
                    text: String(root.s.listenPort)
                    font.family: Theme.fontMono
                    font.pixelSize: 12
                    color: Theme.fg2
                    inputMethodHints: Qt.ImhDigitsOnly
                    width: 54
                    validator: IntValidator { bottom: 1024; top: 65535 }
                    selectByMouse: true
                    onEditingFinished: {
                        const p = parseInt(text)
                        if (p >= 1024 && p <= 65535) root.s.listenPort = p
                        else text = String(root.s.listenPort)
                    }
                    Connections {
                        target: root.s
                        function onListenPortChanged() {
                            if (!portInput.activeFocus)
                                portInput.text = String(root.s.listenPort)
                        }
                    }
                }
            }
            VcSettingRow {
                icon: "reconnect"
                title: qsTr("Auto-reconnect")
                description: qsTr("Re-pair when a device drops off")
                VcToggle {
                    checked: root.s.autoReconnect
                    onToggled: (c) => root.s.autoReconnect = c
                }
            }
            VcSettingRow {
                icon: "shield"
                title: qsTr("Restrict to local subnet")
                description: qsTr("Only allow same-network devices")
                VcToggle {
                    checked: root.s.restrictSubnet
                    onToggled: (c) => root.s.restrictSubnet = c
                }
            }
        }
        Item { width: 1; height: 24 }

        VcCard {
            width: parent.width
            header: qsTr("Updates")

            // Channel — Stable / Beta. Persisted in UpdateChecker (rebuilds the
            // manifest URL); changing it triggers a re-check.
            VcSettingRow {
                showDivider: false
                icon: "protocol"; accent: true
                title: qsTr("Update channel")
                description: qsTr("Stable ships tested builds · Beta gets them early")
                VcSeg {
                    model: [qsTr("Stable"), qsTr("Beta")]
                    currentIndex: UpdateChecker.channel === "beta" ? 1 : 0
                    onActivated: (i) => UpdateChecker.channel = (i === 1 ? "beta" : "stable")
                }
            }

            // Automatic updates → setAutoDownload(). When on, an available
            // update downloads in the background without a prompt.
            VcSettingRow {
                icon: "reconnect"
                title: qsTr("Automatic updates")
                description: qsTr("Download new versions in the background")
                VcToggle {
                    checked: root.s.autoUpdate
                    onToggled: (c) => root.s.autoUpdate = c
                }
            }

            // Check frequency — drives the periodic timer in UpdateBanner.
            VcSettingRow {
                icon: "clock"
                title: qsTr("Check frequency")
                description: qsTr("How often %1 looks for updates").arg(AppInfo.displayName)
                VcSeg {
                    model: [qsTr("On launch"), qsTr("Daily"), qsTr("Weekly")]
                    currentIndex: root.s.updateFrequency
                    onActivated: (i) => root.s.updateFrequency = i
                }
            }

            // Status line + manual check.
            VcSettingRow {
                icon: "info"
                title: qsTr("Version %1").arg(UpdateChecker.currentVersion)
                description: UpdateChecker.statusText !== ""
                             ? UpdateChecker.statusText
                             : (UpdateChecker.installable
                                ? qsTr("Up to date")
                                : qsTr("Managed install — update from viewcam.tech"))
                VcButton {
                    kind: "soft"
                    text: UpdateChecker.state === UpdateChecker.Checking
                          ? qsTr("Checking…") : qsTr("Check now")
                    onClicked: UpdateChecker.checkNow()
                }
            }
        }
        Item { width: 1; height: 24 }

        VcCard {
            width: parent.width
            header: qsTr("Appearance")

            VcSettingRow {
                showDivider: false
                icon: "sun"; accent: true
                title: qsTr("Theme")
                description: qsTr("Studio appearance")
                VcSeg {
                    model: [qsTr("Dark"), qsTr("Light")]
                    currentIndex: root.s.darkTheme ? 0 : 1
                    onActivated: (i) => root.s.darkTheme = (i === 0)
                }
            }
            VcSettingRow {
                icon: "globe"
                title: qsTr("Language")
                description: qsTr("App display language")

                Item {
                    id: langPicker
                    readonly property var codes:  ["",          "en",      "tr",       "de",       "fr",        "ar",       "hi",     "zh",   "es",       "ru",        "uk"          ]
                    readonly property var labels: [qsTr("System"), "English", "Türkçe", "Deutsch", "Français", "العربية", "हिन्दी", "中文", "Español", "Русский", "Українська"]

                    width: langBtn.implicitWidth
                    height: langBtn.implicitHeight

                    Rectangle {
                        id: langBtn
                        implicitWidth: langBtnLabel.implicitWidth + 32
                        implicitHeight: 30
                        radius: 8
                        color: langBtnArea.containsMouse ? Theme.bg3 : Theme.bg2
                        border.color: Theme.line1

                        Row {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                id: langBtnLabel
                                text: {
                                    const idx = langPicker.codes.indexOf(root.s.language)
                                    return idx < 0 ? langPicker.labels[0] : langPicker.labels[idx]
                                }
                                font.family: Theme.fontSans
                                font.pixelSize: 13
                                color: Theme.fg1
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "▾"
                                font.pixelSize: 11
                                color: Theme.fg3
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                        MouseArea {
                            id: langBtnArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: langPopup.open()
                        }
                    }

                    Popup {
                        id: langPopup
                        parent: langBtn
                        y: parent.height + 4
                        x: parent.width - width
                        padding: 6
                        background: Rectangle {
                            color: Theme.bg2
                            radius: Theme.radiusMd
                            border.color: Theme.line1
                        }
                        Column {
                            spacing: 2
                            Repeater {
                                model: langPicker.labels
                                delegate: Rectangle {
                                    required property string modelData
                                    required property int index
                                    readonly property bool sel: langPicker.codes[index] === root.s.language
                                    width: 170; height: 32; radius: 6
                                    color: rowHover.containsMouse ? Theme.bg3 : (sel ? Theme.irisSoft : "transparent")
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.left: parent.left
                                        anchors.leftMargin: 12
                                        text: modelData
                                        font.family: Theme.fontSans
                                        font.pixelSize: 13
                                        color: sel ? Theme.iris : Theme.fg1
                                    }
                                    MouseArea {
                                        id: rowHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: { root.s.language = langPicker.codes[index]; langPopup.close() }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            VcSettingRow {
                icon: "palette"
                title: qsTr("Accent color")
                description: qsTr("Highlights and active states")
                Row {
                    spacing: 9

                    Repeater {
                        model: ["#8B7CFF", "#5DE0A0", "#5388DF", "#F5C56B"]

                        Rectangle {
                            id: swatch
                            required property string modelData

                            readonly property bool selected:
                                Theme.accentOverride.toUpperCase() === modelData.toUpperCase()

                            width: 26; height: 26; radius: 13
                            color: modelData
                            border.width: 1
                            border.color: Theme.line2

                            // selection ring: 2px gap + 2px accent halo
                            Rectangle {
                                anchors.centerIn: parent
                                width: 34; height: 34; radius: 17
                                visible: swatch.selected
                                color: "transparent"
                                border.width: 2
                                border.color: swatch.modelData
                            }
                            Rectangle {
                                anchors.centerIn: parent
                                width: 30; height: 30; radius: 15
                                visible: swatch.selected
                                color: "transparent"
                                border.width: 2
                                border.color: Theme.bg2
                            }

                            TapHandler {
                                onTapped: root.s.accentColor = swatch.modelData
                            }
                        }
                    }
                }
            }
            VcSettingRow {
                icon: "telemetry"
                title: qsTr("Telemetry overlay")
                description: qsTr("Bitrate, FPS and latency on the viewfinder")
                VcToggle {
                    checked: root.s.telemetryOverlay
                    onToggled: (c) => root.s.telemetryOverlay = c
                }
            }
            VcSettingRow {
                icon: "compact"
                title: qsTr("Compact sidebar")
                description: qsTr("Denser navigation and device list")
                VcToggle {
                    checked: root.s.compactSidebar
                    onToggled: (c) => root.s.compactSidebar = c
                }
            }
            VcSettingRow {
                icon: "laptop"
                title: qsTr("Launch at login")
                description: qsTr("Start ViewCam in the background")
                VcToggle {
                    checked: root.s.launchAtLogin
                    onToggled: (c) => root.s.launchAtLogin = c
                }
            }
        }
        Item { width: 1; height: 24 }

        VcCard {
            width: parent.width
            header: qsTr("About")

            VcSettingRow {
                showDivider: false
                icon: "globe"; accent: true
                title: qsTr("ViewCam website")
                description: "viewcam.tech"
                VcButton {
                    kind: "soft"
                    text: qsTr("Open")
                    onClicked: Qt.openUrlExternally("https://viewcam.tech")
                }
            }
            VcSettingRow {
                icon: "info"
                title: AppInfo.displayName
                description: qsTr("Version %1 · build %2").arg(AppInfo.versionString).arg(AppInfo.buildNumber)
                VcButton {
                    kind: "soft"
                    text: qsTr("Check for updates")
                    onClicked: UpdateChecker.checkNow()
                }
            }
            VcSettingRow {
                icon: "code"
                title: qsTr("Qt runtime")
                description: qsTr("Cross-platform framework")
                Text {
                    text: qsTr("Qt %1").arg(AppInfo.qtRuntimeVersion)
                    font.family: Theme.fontMono
                    font.pixelSize: 12
                    color: Theme.fg2
                }
            }
            VcSettingRow {
                visible: AppController.cudaVersion !== ""
                icon: "gpu"
                title: qsTr("CUDA runtime")
                description: qsTr("NVIDIA parallel computing platform")
                Text {
                    text: AppController.cudaVersion
                    font.family: Theme.fontMono
                    font.pixelSize: 12
                    color: Theme.fg2
                }
            }
            VcSettingRow {
                id: rendererRow
                icon: "renderer"
                title: qsTr("Renderer")
                description: qsTr("Graphics backend")
                Text {
                    text: {
                        switch (rendererRow.GraphicsInfo.api) {
                        case GraphicsInfo.OpenGL: return "RHI · OpenGL"
                        case GraphicsInfo.Vulkan: return "RHI · Vulkan"
                        case GraphicsInfo.Metal: return "RHI · Metal"
                        case GraphicsInfo.Direct3D11: return "RHI · D3D11"
                        case GraphicsInfo.Direct3D12: return "RHI · D3D12"
                        case GraphicsInfo.Software: return "Software"
                        default: return "RHI"
                        }
                    }
                    font.family: Theme.fontMono
                    font.pixelSize: 12
                    color: Theme.fg2
                }
            }
            VcSettingRow {
                icon: "licenses"
                title: qsTr("Open-source licenses")
                description: qsTr("Third-party components")
                VcIcon {
                    width: 18; height: 18
                    name: "chevron-down"
                    color: Theme.fg3
                    rotation: -90
                }
            }
        }
    }
}
