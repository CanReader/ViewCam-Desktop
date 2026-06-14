pragma Singleton
import QtQuick
import ViewCam.Studio

// ViewCam design tokens — 1:1 port of colors_and_type.css.
// Dark-first; light variant included. Accent (iris) is runtime-overridable
// from Settings → Appearance.
QtObject {
    id: theme

    // Live-bound to persisted settings (same-module singleton)
    property bool dark: AppController.settings.darkTheme
    property string accentOverride: AppController.settings.accentColor

    // ---------- Color · surfaces ----------
    readonly property color bg0: dark ? "#08080B" : "#FBFAF6"
    readonly property color bg1: dark ? "#0B0B0E" : "#F4F3EE"
    readonly property color bg2: dark ? "#15151A" : "#FFFFFF"
    readonly property color bg3: dark ? "#1E1E25" : "#EDECE6"
    readonly property color bg4: dark ? "#272730" : "#E2E1DA"

    readonly property color line1: dark ? "#2A2A33" : Qt.rgba(11/255, 11/255, 14/255, 0.08)
    readonly property color line2: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(11/255, 11/255, 14/255, 0.12)
    readonly property color line3: dark ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(11/255, 11/255, 14/255, 0.20)

    readonly property color fg1: dark ? "#F2F1ED" : "#14141A"
    readonly property color fg2: dark ? "#9A9AA5" : "#5C5C66"
    readonly property color fg3: dark ? "#5C5C66" : "#8C8C96"
    readonly property color fg4: dark ? "#3A3A42" : "#BFBFC8"

    // ---------- Iris — the signature accent ----------
    // Default iris deepens in light mode for AA contrast; custom accents
    // picked from the swatches pass through unchanged (mockup behavior).
    readonly property color iris: {
        if (accentOverride.toUpperCase() === "#8B7CFF")
            return dark ? "#8B7CFF" : "#6E5CE6"
        return accentOverride
    }
    readonly property color irisSoft: Qt.rgba(iris.r, iris.g, iris.b, dark ? 0.16 : 0.10)
    readonly property color irisGlow: Qt.rgba(iris.r, iris.g, iris.b, dark ? 0.40 : 0.28)
    readonly property color irisMesh: Qt.rgba(iris.r, iris.g, iris.b, dark ? 0.06 : 0.05)

    // ---------- Signal (rationed, destructive/REC only) ----------
    readonly property color signal: dark ? "#FF6B6B" : "#D94F4F"
    readonly property color signalSoft: dark ? Qt.rgba(1, 107/255, 107/255, 0.14) : Qt.rgba(217/255, 79/255, 79/255, 0.10)
    readonly property color signalGlow: dark ? Qt.rgba(1, 107/255, 107/255, 0.35) : Qt.rgba(217/255, 79/255, 79/255, 0.28)

    // ---------- Status — dots, pills, 1-2px borders. Never fills ----------
    readonly property color statusLive: dark ? "#5DE0A0" : "#2EAE74"
    readonly property color statusReady: statusLive
    readonly property color statusRec: dark ? "#FF6B6B" : "#D94F4F"
    readonly property color statusWarn: dark ? "#F5C56B" : "#B8861A"
    readonly property color statusError: dark ? "#FF8A8A" : "#D94F4F"
    readonly property color statusInfo: iris

    // Extra hues used by the desktop mockup chrome
    readonly property color slate: "#64748B"
    readonly property color blue: "#5388DF"

    // ---------- Glass — HUD over live video only ----------
    readonly property color glassBg: dark ? Qt.rgba(18/255, 19/255, 23/255, 0.62) : Qt.rgba(1, 1, 1, 0.70)
    readonly property color glassBorder: dark ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(10/255, 11/255, 14/255, 0.10)

    // ---------- Type ----------
    readonly property string fontSans: "Geist"
    readonly property string fontMono: "Geist Mono"

    readonly property int textXs: 11
    readonly property int textSm: 13
    readonly property int textBase: 15
    readonly property int textMd: 17
    readonly property int textLg: 22
    readonly property int textXl: 28
    readonly property int text2xl: 38
    readonly property int text3xl: 56
    readonly property int text4xl: 80

    readonly property real leadingTight: 1.1
    readonly property real leadingSnug: 1.25
    readonly property real leadingNormal: 1.45
    readonly property real leadingRelaxed: 1.6

    // letter-spacing in px at given size: em * fontSize
    function tracking(em, px) { return em * px }
    readonly property real trackingTight: -0.02
    readonly property real trackingWide: 0.02
    readonly property real trackingMono: 0.04

    // ---------- Spacing (4px base) ----------
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24
    readonly property int s6: 32
    readonly property int s7: 48
    readonly property int s8: 64
    readonly property int s9: 96

    // ---------- Radius ----------
    readonly property int radiusXs: 2
    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 10
    readonly property int radiusXl: 14
    readonly property int radiusPill: 999

    // ---------- Motion: durations (ms) ----------
    readonly property int durInstant: 80
    readonly property int durSnap: 160
    readonly property int durPull: 320
    readonly property int durIris: 520
    readonly property int durRamp: 900

    // ---------- Motion: the four named eases ----------
    // cubic-bezier control points for Easing.BezierSpline
    readonly property var easeIris: [0.22, 0.61, 0.36, 1, 1, 1]
    readonly property var easePull: [0.65, 0.05, 0.36, 1, 1, 1]
    readonly property var easeRamp: [0.4, 0.0, 0.2, 1, 1, 1]
    readonly property var easeSnap: [0.2, 0.9, 0.3, 1, 1, 1]

    // ---------- Layout ----------
    readonly property int sidebarW: 280
    readonly property int rightPanelW: 344
    readonly property int contentMax: 760

    // ---------- Helpers ----------
    readonly property string assetBase: "qrc:/qt/qml/ViewCam/Studio/resources/"
    function icon(name) { return assetBase + "icons/" + name + ".svg" }
    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
}
