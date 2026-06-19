import QtQuick
import ViewCam.Studio

// Center stage: fullbleed viewfinder with empty state or live video + HUD.
Item {
    id: root

    signal openLauncher

    property bool micOff: AppController.audio.micMuted
    property real volume: 0.6

    readonly property var conn: AppController.connection

    // viewfinder — a camera monitor: deliberately a flat, neutral charcoal in
    // both themes (no warm/brown tones). Empty state reads as "standby / waiting
    // for signal", not an off TV.
    Rectangle {
        id: viewfinder
        anchors.fill: parent
        anchors.margins: 18
        radius: Theme.radiusXl
        border.width: 1
        border.color: Theme.line1
        clip: true

        // flat charcoal base with a faint top-down lift for depth
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: "#101016"
            }
            GradientStop {
                position: 1.0
                color: "#0A0A0E"
            }
        }

        // soft iris standby glow behind the empty state — gives the "no signal"
        // surface intent. Tracks the runtime accent.
        Canvas {
            id: standbyGlow
            anchors.fill: parent
            visible: !root.conn.connected
            property color glow: Theme.iris
            onGlowChanged: requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const w = width, h = height;
                ctx.save();
                ctx.translate(w * 0.5, h * 0.42);
                ctx.scale(1.35, h / w);
                const rg = ctx.createRadialGradient(0, 0, 0, 0, 0, w * 0.6);
                rg.addColorStop(0, Qt.rgba(glow.r, glow.g, glow.b, 0.10));
                rg.addColorStop(0.6, Qt.rgba(glow.r, glow.g, glow.b, 0.02));
                rg.addColorStop(1, Qt.rgba(glow.r, glow.g, glow.b, 0));
                ctx.fillStyle = rg;
                ctx.fillRect(-w * 2, -h * 2, w * 4, h * 4);
                ctx.restore();
            }
        }

        // live video
        FrameView {
            id: frameView
            anchors.fill: parent
            source: AppController.frameSource
            mirror: AppController.settings.mirrorImage
            visible: root.conn.connected && hasFrame
        }

        // vignette
        Canvas {
            anchors.fill: parent
            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const w = width, h = height;
                ctx.save();
                ctx.translate(w * 0.5, h * 0.45);
                ctx.scale(1.2, h / w);
                const rg = ctx.createRadialGradient(0, 0, 0, 0, 0, w);
                rg.addColorStop(0.45, "rgba(0,0,0,0)");
                rg.addColorStop(1, "rgba(0,0,0,0.55)");
                ctx.fillStyle = rg;
                ctx.fillRect(-w * 2, -h * 2, w * 4, h * 4);
                ctx.restore();
            }
        }

        // ── empty state (disconnected) ───────────────────────────
        Column {
            anchors.centerIn: parent
            visible: !root.conn.connected
            spacing: 0

            VcIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 96
                height: 96
                name: "camera-thin"
                // viewfinder is a dark monitor in both themes → white-alpha is correct
                color: Qt.rgba(1, 1, 1, 0.20)
            }
            Item {
                width: 1
                height: 14
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.conn.sessionLimited ? qsTr("Session ended") : qsTr("No camera connected")
                font.family: Theme.fontSans
                font.pixelSize: 15
                color: Theme.fg2
            }
            Item {
                width: 1
                height: 6
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.conn.sessionLimited ? qsTr("Free tier session limit reached. Reconnect to continue.") : qsTr("Connect a phone running ViewCam to start the live feed.")
                font.family: Theme.fontSans
                font.pixelSize: 13
                color: Theme.fg3
            }
            Item {
                width: 1
                height: 20
            }
            VcButton {
                anchors.horizontalCenter: parent.horizontalCenter
                kind: "primary"
                icon: "search"
                text: qsTr("Scan for devices")
                onClicked: root.openLauncher()
            }
        }

        // ── HUD (connected) ──────────────────────────────────────
        Item {
            anchors.fill: parent
            visible: root.conn.connected

            // LIVE chip — top-left
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 16
                height: 34
                width: liveRow.implicitWidth + 26
                radius: Theme.radiusPill
                color: Theme.glassBg
                border.width: 1
                border.color: Theme.glassBorder

                Row {
                    id: liveRow
                    anchors.centerIn: parent
                    spacing: 9

                    Item {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8

                        Rectangle {
                            anchors.centerIn: parent
                            width: 8
                            height: 8
                            radius: 4
                            color: Theme.statusLive
                        }
                        Rectangle {
                            id: hudRing
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            radius: 8
                            color: "transparent"
                            border.width: 1.5
                            border.color: Theme.statusLive
                            opacity: 0

                            ParallelAnimation {
                                running: root.conn.connected
                                loops: Animation.Infinite
                                NumberAnimation {
                                    target: hudRing
                                    property: "scale"
                                    from: 0.6
                                    to: 1.6
                                    duration: 1800
                                    easing.type: Easing.BezierSpline
                                    easing.bezierCurve: Theme.easeIris
                                }
                                NumberAnimation {
                                    target: hudRing
                                    property: "opacity"
                                    from: 0.8
                                    to: 0
                                    duration: 1800
                                }
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "LIVE"
                        font.family: Theme.fontSans
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.04 * 13
                        color: Theme.fg1
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 1
                        height: 14
                        color: Theme.line3
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.conn.uptimeText
                        font.family: Theme.fontMono
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: Theme.fg2
                    }
                }
            }

            // battery chip — top-right
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 16
                height: 34
                width: 40
                radius: Theme.radiusPill
                color: Theme.glassBg
                border.width: 1
                border.color: Theme.glassBorder

                VcIcon {
                    anchors.centerIn: parent
                    width: 18; height: 18
                    name: root.conn.charging ? "battery-charging"
                          : root.conn.battery >= 60 ? "battery-full"
                          : root.conn.battery >= 20 ? "battery-mid"
                          : "battery-low"
                    color: root.conn.battery < 20 ? Theme.statusError : Theme.fg1
                }
            }

            // watermark — top-right, below battery chip (free tier)
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 58
                anchors.rightMargin: 16
                height: 28
                width: wmRow.implicitWidth + 18
                radius: Theme.radiusPill
                color: Theme.glassBg
                border.width: 1
                border.color: Theme.glassBorder

                Row {
                    id: wmRow
                    anchors.centerIn: parent
                    spacing: 6

                    Monogram {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 14
                        height: 14
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "ViewCam"
                        font.family: Theme.fontSans
                        font.pixelSize: 12
                        font.weight: Font.SemiBold
                        font.letterSpacing: 0.04 * 12
                        color: "white"
                    }
                }
            }

            // telemetry badges — above the control bar
            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: 16
                anchors.bottomMargin: 88
                spacing: 8
                visible: AppController.settings.telemetryOverlay

                // unknown numerics show "—", never a fake 0 / raw sentinel
                VcBadge {
                    text: root.conn.frameHeight >= 2160 ? "4K" : (root.conn.frameHeight > 0 ? root.conn.frameHeight + "P" : "—")
                }
                VcBadge {
                    text: root.conn.fps > 0 ? root.conn.fps + " FPS" : "—"
                }
                VcBadge {
                    text: root.conn.bitrateMbps > 0 ? root.conn.bitrateMbps.toFixed(1) + " Mbps" : "—"
                }
                VcBadge {
                    text: root.conn.frameIntervalMs > 0 ? root.conn.frameIntervalMs.toFixed(0) + " ms" : "—"
                    good: root.conn.frameIntervalMs > 0 && root.conn.frameIntervalMs < 34
                }
            }

            // bottom control bar
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 16
                height: 60
                radius: Theme.radiusXl
                color: Theme.glassBg
                border.width: 1
                border.color: Theme.glassBorder

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 16

                    VcIconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        icon: "mic"
                        off: root.micOff
                        onClicked: AppController.audio.micMuted = !root.micOff
                    }

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 10

                        VcIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 16
                            height: 16
                            name: "volume"
                            color: Theme.fg3
                        }
                        VcSlider {
                            anchors.verticalCenter: parent.verticalCenter
                            value: root.volume
                            onMoved: v => root.volume = v
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Virtual Camera")
                        font.family: Theme.fontSans
                        font.pixelSize: 14
                        color: Theme.fg1
                    }
                    VcToggle {
                        anchors.verticalCenter: parent.verticalCenter
                        checked: AppController.virtualCam.enabled
                        onToggled: c => AppController.virtualCam.enabled = c
                    }
                }
            }
        }
    }
}
