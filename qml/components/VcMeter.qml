import QtQuick
import ViewCam.Studio

// Audio level meter: thin mint bars. Two modes:
//  - level >= 0: bars track the REAL signal level (0..1) with per-bar shaping,
//    fast attack / eased release — used while phone mic audio actually flows.
//  - level < 0 (default): the decorative animation, randomized phase cycling
//    20% -> 100%, exactly like the mockup's CSS keyframes.
// The decorative animation drives a per-bar `wavePhase` property, NEVER the
// height itself: a NumberAnimation writing height would sever its binding and
// freeze the bar the moment the mode flips to live.
Row {
    id: root

    property int bars: 14
    property bool running: true
    // Live signal level 0..1; -1 keeps the decorative animation.
    property real level: -1

    readonly property bool live: level >= 0

    spacing: 3
    height: 28

    Repeater {
        model: root.bars

        Item {
            id: barSlot
            required property int index
            // Static per-bar shape so a steady level still reads as a natural
            // spectrum instead of a flat block.
            readonly property real shape: 0.45 + 0.55 * Math.abs(Math.sin(index * 2.7 + 0.8))
            // 0..1 sweep driven by the decorative animation.
            property real wavePhase: 0

            width: 3
            height: root.height

            SequentialAnimation on wavePhase {
                running: !root.live && root.running && root.visible

                PauseAnimation { duration: Math.floor(Math.random() * 1100) }
                SequentialAnimation {
                    loops: Animation.Infinite
                    NumberAnimation { to: 1; duration: 550 }
                    NumberAnimation { to: 0; duration: 550 }
                }
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: 3
                radius: 2
                color: Theme.statusLive
                opacity: root.live && !root.running ? 0.35 : 0.85
                height: parent.height * (root.live
                    ? 0.12 + 0.88 * Math.min(1, Math.max(0, root.level)) * barSlot.shape
                    : 0.2 + 0.8 * barSlot.wavePhase)

                Behavior on height {
                    enabled: root.live
                    NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
                }
            }
        }
    }
}
