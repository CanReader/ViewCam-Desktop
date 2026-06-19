import QtQuick
import ViewCam.Studio

// Startup overlay shown while the update check runs. Fades out once the
// checker reaches UpToDate or Error so the normal Launcher slides in.
Item {
    id: root

    readonly property bool checkDone: UpdateChecker.state === UpdateChecker.UpToDate
                                   || UpdateChecker.state === UpdateChecker.Error
    property bool timerDone: false
    readonly property bool done: checkDone && timerDone

    // Base 2s minimum so the splash is never just a flash.
    Timer {
        id: baseTimer
        interval: 2000
        running: true
        repeat: false
        onTriggered: {
            if (!root.checkDone) return          // still waiting for network — onCheckDoneChanged will finish
            if (UpdateChecker.statusText !== "")
                msgTimer.restart()               // extra time to read the message
            else
                root.timerDone = true
        }
    }

    // 1.5 s extra when there is a message to read (total ≥ 3.5 s from launch).
    Timer {
        id: msgTimer
        interval: 1500
        running: false
        repeat: false
        onTriggered: root.timerDone = true
    }

    // If the network reply arrives after the base timer already fired,
    // give 1.5 s from that point so the message is still readable.
    onCheckDoneChanged: {
        if (checkDone && !baseTimer.running) {
            if (UpdateChecker.statusText !== "")
                msgTimer.restart()
            else
                timerDone = true
        }
    }

    opacity: done ? 0 : 1
    visible: opacity > 0

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
            color: root.checkDone ? Theme.fg1 : Theme.fg3
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

        // Pulsing dot — visible while checking or applying
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 6; height: 6; radius: 3
            color: Theme.iris
            visible: UpdateChecker.state === UpdateChecker.Checking
                  || UpdateChecker.state === UpdateChecker.Applying

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 1;    duration: 600; easing.type: Easing.InOutSine }
                NumberAnimation { to: 0.25; duration: 600; easing.type: Easing.InOutSine }
            }
        }
    }
}
