#include "viewmodels/CameraControlViewModel.h"
#include "core/Logger.h"

CameraControlViewModel::CameraControlViewModel(QObject *parent)
    : QObject(parent) {}

void CameraControlViewModel::setTorch(bool v) {
    if (m_torch == v)
        return;
    m_torch = v;
    emit torchChanged();
    emit controlPatch(QJsonObject{{"torch", v}});
}

void CameraControlViewModel::setFocusLock(bool v) {
    if (m_focusLock == v)
        return;
    m_focusLock = v;
    emit focusLockChanged();
    emit controlPatch(QJsonObject{{"focusLock", v}});
}

void CameraControlViewModel::setExposureLock(bool v) {
    if (m_exposureLock == v)
        return;
    m_exposureLock = v;
    emit exposureLockChanged();
    emit controlPatch(QJsonObject{{"exposureLock", v}});
}

void CameraControlViewModel::setHdr(bool v) {
    if (m_hdr == v)
        return;
    m_hdr = v;
    emit hdrChanged();
    emit controlPatch(QJsonObject{{"hdr", v}});
}

QJsonObject CameraControlViewModel::snapshot() const {
    return QJsonObject{{"torch", m_torch},
                       {"focusLock", m_focusLock},
                       {"exposureLock", m_exposureLock},
                       {"hdr", m_hdr}};
}

void CameraControlViewModel::applyControls(const QJsonObject &controls) {
    // Phone is the source of truth — adopt only the keys it actually reports,
    // emitting *Changed (never controlPatch, to avoid an echo loop).
    if (controls.contains("torch")) {
        const bool v = controls.value("torch").toBool();
        if (v != m_torch) { m_torch = v; emit torchChanged(); }
    }
    if (controls.contains("focusLock")) {
        const bool v = controls.value("focusLock").toBool();
        if (v != m_focusLock) { m_focusLock = v; emit focusLockChanged(); }
    }
    if (controls.contains("exposureLock")) {
        const bool v = controls.value("exposureLock").toBool();
        if (v != m_exposureLock) { m_exposureLock = v; emit exposureLockChanged(); }
    }
    if (controls.contains("hdr")) {
        const bool v = controls.value("hdr").toBool();
        if (v != m_hdr) { m_hdr = v; emit hdrChanged(); }
    }
    if (controls.contains("hdrSupported")) {
        const bool v = controls.value("hdrSupported").toBool();
        if (v != m_hdrSupported) { m_hdrSupported = v; emit hdrSupportedChanged(); }
    }
    if (controls.contains("torchAvailable")) {
        const bool v = controls.value("torchAvailable").toBool();
        if (v != m_torchAvailable) { m_torchAvailable = v; emit torchAvailableChanged(); }
    }
}

void CameraControlViewModel::reset() {
    // Back to panel defaults without emitting controlPatch (we're disconnecting).
    if (m_torch) { m_torch = false; emit torchChanged(); }
    if (m_focusLock) { m_focusLock = false; emit focusLockChanged(); }
    if (!m_exposureLock) { m_exposureLock = true; emit exposureLockChanged(); }
    if (!m_hdr) { m_hdr = true; emit hdrChanged(); }
    if (!m_hdrSupported) { m_hdrSupported = true; emit hdrSupportedChanged(); }
    if (!m_torchAvailable) { m_torchAvailable = true; emit torchAvailableChanged(); }
}
