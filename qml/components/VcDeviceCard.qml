import QtQuick
import QtQuick.Layouts
import ViewCam.Studio

// Launcher device card (.dcard): phone glyph, name, ip · TCP port, go arrow.
// Hover: bg steps up, border tints iris, lifts 1px. Entrance fade-up is
// driven by the parent via `appearDelay`.
Rectangle {
    id: root

    property string deviceName: ""
    property string host: ""
    property int port: 0   // always supplied by the device model
    property int appearDelay: 0
    signal clicked()

    implicitHeight: 82
    radius: Theme.radiusXl
    color: hover.hovered ? Theme.bg3 : Theme.bg2
    border.width: 1
    border.color: hover.hovered ? Theme.alpha(Theme.iris, 0.35) : Theme.line1

    Behavior on color { ColorAnimation { duration: Theme.durSnap } }
    Behavior on border.color { ColorAnimation { duration: Theme.durSnap } }

    transform: [
        Translate { id: enterTranslate; y: 8 },
        Translate { y: hover.hovered ? -1 : 0; Behavior on y { NumberAnimation { duration: Theme.durSnap } } }
    ]

    // entrance: fadeUp with stagger
    opacity: 0
    Component.onCompleted: appear.start()
    SequentialAnimation {
        id: appear
        PauseAnimation { duration: root.appearDelay }
        ParallelAnimation {
            NumberAnimation {
                target: root; property: "opacity"; from: 0; to: 1
                duration: Theme.durPull
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeIris
            }
            NumberAnimation {
                target: enterTranslate; property: "y"; from: 8; to: 0
                duration: Theme.durPull
                easing.type: Easing.BezierSpline
                easing.bezierCurve: Theme.easeIris
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 14

        VcPhoneGlyph {
            Layout.preferredWidth: 36
            Layout.preferredHeight: 52
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: root.deviceName
                font.family: Theme.fontSans
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.fg1
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: root.host + " · TCP " + root.port
                font.family: Theme.fontMono
                font.pixelSize: 12
                color: Theme.fg3
                elide: Text.ElideRight
            }
        }

        VcIcon {
            width: 18
            height: 18
            name: "arrow-right"
            color: hover.hovered ? Theme.iris : Theme.fg3
        }
    }

    HoverHandler { id: hover }
    TapHandler { onTapped: root.clicked() }
}
