import QtQuick
import ViewCam.Studio

// Apply-moment overlay. Phase 5: the background CHECK no longer blocks the
// splash — this screen only takes over while an update is actually being
// installed (Downloading / Verifying / Applying), e.g. an "Install now" that
// the user triggered, or a resumed apply at launch. The rest of the time it
// is fully transparent and lets the normal app through.
Item {
    id: root

    readonly property bool installing:
           UpdateChecker.state === UpdateChecker.Downloading
        || UpdateChecker.state === UpdateChecker.Verifying
        || UpdateChecker.state === UpdateChecker.Applying

    opacity: installing ? 1 : 0
    visible: opacity > 0
    // Soak input while the apply overlay is up so the app underneath is inert.
    enabled: installing

    MouseArea {
        anchors.fill: parent
        enabled: root.installing
        hoverEnabled: root.installing
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 300
            easing.type: Easing.BezierSpline
            easing.bezierCurve: Theme.easeIris
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg1
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Monogram {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 56
            height: 56
        }

        Item { width: 1; height: 20 }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: AppInfo.displayName
            font.family: Theme.fontSans
            font.pixelSize: 22
            font.weight: Font.DemiBold
            font.letterSpacing: -0.01 * 22
            color: Theme.fg1
        }

        Item { width: 1; height: 36 }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: UpdateChecker.statusText
            font.family: Theme.fontMono
            font.pixelSize: 12
            font.letterSpacing: 0.04 * 12
            color: Theme.fg1
            visible: UpdateChecker.statusText !== ""
        }

        Item { width: 1; height: 14 }

        // Progress bar — visible only while downloading
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 200
            height: 3
            radius: 2
            color: Theme.line2
            visible: UpdateChecker.state === UpdateChecker.Downloading

            Rectangle {
                width: parent.width * (UpdateChecker.progress / 100.0)
                height: parent.height
                radius: 2
                color: Theme.iris

                Behavior on width {
                    NumberAnimation { duration: 80 }
                }
            }
        }

        // Pulsing dot — visible while verifying or applying (no progress %)
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 6; height: 6; radius: 3
            color: Theme.iris
            visible: UpdateChecker.state === UpdateChecker.Verifying
                  || UpdateChecker.state === UpdateChecker.Applying

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 1;    duration: 600; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.25; duration: 600; easing.type: Easing.InOutSine }
            }
        }
    }
}
