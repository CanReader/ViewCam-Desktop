#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Desktop-side mirror of the phone's camera-control state (Torch / Focus lock /
// Exposure lock / HDR). QML toggles call the Q_INVOKABLE setters, which update
// the local "desired" value and emit controlPatch() — AppController forwards
// that as a CONTROL frame to the phone. The phone is the source of truth: when
// it echoes applied state in STATUS controls{}, applyControls() reconciles the
// UI (and disables unsupported toggles, e.g. hdrSupported=false).
// Keys match CONNECTIVITY_PROTOCOL.md §4 exactly.
class CameraControlViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(bool torch READ torch NOTIFY torchChanged)
    Q_PROPERTY(bool focusLock READ focusLock NOTIFY focusLockChanged)
    Q_PROPERTY(bool exposureLock READ exposureLock NOTIFY exposureLockChanged)
    Q_PROPERTY(bool hdr READ hdr NOTIFY hdrChanged)
    Q_PROPERTY(bool hdrSupported READ hdrSupported NOTIFY hdrSupportedChanged)
    // False when the active camera has no flash unit (e.g. front camera), echoed
    // by the phone in STATUS controls{}.torchAvailable. Updates live on lens flip.
    Q_PROPERTY(bool torchAvailable READ torchAvailable NOTIFY torchAvailableChanged)
    // 0=480p, 1=720p, 2=1080p — drives the Desktop resolution VcSeg.
    Q_PROPERTY(int resolutionIndex READ resolutionIndex NOTIFY resolutionIndexChanged)
    // Echoed from STATUS controls{}.lens — tracks which camera is active on the phone.
    Q_PROPERTY(bool lensFront READ lensFront NOTIFY lensFrontChanged)
    // Output aspect ratio: "full" (native) | "16:9" | "4:3" | "1:1" | "9:16".
    // The phone crops the capture to this before encoding.
    Q_PROPERTY(QString aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)
    // Phone camera zoom. zoom is the applied ratio (1.0 = none); zoomMax is the
    // active lens's ceiling, echoed by the phone in STATUS controls{}.
    Q_PROPERTY(double zoom READ zoom NOTIFY zoomChanged)
    Q_PROPERTY(double zoomMax READ zoomMax NOTIFY zoomMaxChanged)

public:
    explicit CameraControlViewModel(QObject *parent = nullptr);

    bool torch() const { return m_torch; }
    bool focusLock() const { return m_focusLock; }
    bool exposureLock() const { return m_exposureLock; }
    bool hdr() const { return m_hdr; }
    bool hdrSupported() const { return m_hdrSupported; }
    bool torchAvailable() const { return m_torchAvailable; }
    int resolutionIndex() const { return m_resolutionIndex; }
    bool lensFront() const { return m_lensFront; }
    QString aspectRatio() const { return m_aspectRatio; }
    double zoom() const { return m_zoom; }
    double zoomMax() const { return m_zoomMax; }

    // User-initiated changes (from QML). Each updates local state and emits a
    // single-key controlPatch() to push to the phone.
    Q_INVOKABLE void setTorch(bool v);
    Q_INVOKABLE void setFocusLock(bool v);
    Q_INVOKABLE void setExposureLock(bool v);
    Q_INVOKABLE void setHdr(bool v);
    // 0=480p 1=720p 2=1080p — sends {resWidth, resHeight} CONTROL to phone.
    Q_INVOKABLE void setResolution(int index);
    // Toggle front/back camera on the phone.
    Q_INVOKABLE void flipLens();
    // Trigger a one-shot centre-point autofocus on the phone.
    Q_INVOKABLE void triggerFocus();
    // Set the output aspect ratio ("full"|"16:9"|"4:3"|"1:1"|"9:16").
    Q_INVOKABLE void setAspectRatio(const QString &ratio);
    // Multiply zoom by factor (Ctrl+wheel: >1 in, <1 out), clamped to
    // [1, zoomMax]. The CONTROL send is coalesced (~70ms) so a fast wheel
    // burst becomes one frame, not dozens.
    Q_INVOKABLE void zoomBy(double factor);
    // Reset zoom to 1x immediately.
    Q_INVOKABLE void resetZoom();

    // Full snapshot sent once on connect so the phone matches the panel.
    QJsonObject snapshot() const;

    // Reconcile with the phone-acknowledged applied state from STATUS controls{}.
    // Does NOT re-emit controlPatch (no echo loop).
    void applyControls(const QJsonObject &controls);

    // Back to defaults on disconnect.
    void reset();

signals:
    void torchChanged();
    void focusLockChanged();
    void exposureLockChanged();
    void hdrChanged();
    void hdrSupportedChanged();
    void torchAvailableChanged();
    void resolutionIndexChanged();
    void lensFrontChanged();
    void aspectRatioChanged();
    void zoomChanged();
    void zoomMaxChanged();
    // Emitted on a user toggle — AppController turns it into a CONTROL frame.
    void controlPatch(const QJsonObject &patch);

private:
    // Defaults mirror the design panel's initial state.
    bool m_torch = false;
    bool m_focusLock = false;
    bool m_exposureLock = true;
    bool m_hdr = true;
    bool m_hdrSupported = true; // assume supported until the phone says otherwise
    bool m_torchAvailable = true; // assume available until the phone says otherwise
    // Default 720p (index 1): 480p upscaled by meeting apps was the top cause
    // of "stream looks bad". 1080p stays a deliberate user choice (bandwidth).
    int m_resolutionIndex = 1;   // 0=480p, 1=720p, 2=1080p
    bool m_lensFront = false;
    QString m_aspectRatio = QStringLiteral("full"); // native until the user picks
    double m_zoom = 1.0;
    double m_zoomMax = 4.0; // conservative default until the phone reports
    QTimer *m_zoomSendTimer = nullptr; // coalesces wheel bursts into one CONTROL

    static constexpr int RES_WIDTHS[]  = {640, 1280, 1920};
    static constexpr int RES_HEIGHTS[] = {480,  720, 1080};
    static constexpr int RES_COUNT     = 3;
};
