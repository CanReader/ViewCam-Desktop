import QtQuick
import ViewCam.Studio

// Animated audio level meter: thin mint bars cycling 20% -> 100% with
// randomized phase, exactly like the mockup's CSS keyframes.
Row {
    id: root

    property int bars: 14
    property bool running: true

    spacing: 3
    height: 28

    Repeater {
        model: root.bars

        Item {
            width: 3
            height: root.height

            Rectangle {
                id: bar
                anchors.bottom: parent.bottom
                width: 3
                radius: 2
                color: Theme.statusLive
                opacity: 0.85
                height: parent.height * 0.2

                SequentialAnimation {
                    running: root.running && root.visible
                    loops: Animation.Infinite

                    PauseAnimation { duration: Math.floor(Math.random() * 1100) }
                    SequentialAnimation {
                        loops: Animation.Infinite
                        NumberAnimation {
                            target: bar; property: "height"
                            to: bar.parent.height
                            duration: 550
                        }
                        NumberAnimation {
                            target: bar; property: "height"
                            to: bar.parent.height * 0.2
                            duration: 550
                        }
                    }
                }
            }
        }
    }
}
