import QtQuick
import ViewCam.Studio

// Settings page: stream engine, network, appearance, about.
Flickable {
    id: root

    readonly property var s: AppController.settings

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
                VcToggle {
                    checked: root.s.gpuProcessing
                    onToggled: (c) => root.s.gpuProcessing = c
                }
            }
            VcSettingRow {
                icon: "protocol"
                title: qsTr("Stream protocol")
                description: qsTr("MJPEG active · H.264/H.265 require mobile update")
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
                VcSeg {
                    readonly property var codes:   ["",   "en",     "tr",     "de",     "fr"]
                    readonly property var labels:  [qsTr("System"), "EN", "TR", "DE", "FR"]
                    model: labels
                    currentIndex: {
                        const idx = codes.indexOf(root.s.language)
                        return idx < 0 ? 0 : idx
                    }
                    onActivated: (i) => root.s.language = codes[i]
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
                icon: "info"; accent: true
                title: AppInfo.displayName
                description: qsTr("Version %1 · build %2").arg(AppInfo.versionString).arg(AppInfo.buildNumber)
                VcButton {
                    kind: "soft"
                    text: qsTr("Check for updates")
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
