#pragma once

#include <QJsonObject>
#include <QObject>
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

public:
    explicit CameraControlViewModel(QObject *parent = nullptr);

    bool torch() const { return m_torch; }
    bool focusLock() const { return m_focusLock; }
    bool exposureLock() const { return m_exposureLock; }
    bool hdr() const { return m_hdr; }
    bool hdrSupported() const { return m_hdrSupported; }
    bool torchAvailable() const { return m_torchAvailable; }

    // User-initiated changes (from QML). Each updates local state and emits a
    // single-key controlPatch() to push to the phone.
    Q_INVOKABLE void setTorch(bool v);
    Q_INVOKABLE void setFocusLock(bool v);
    Q_INVOKABLE void setExposureLock(bool v);
    Q_INVOKABLE void setHdr(bool v);

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
};
