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
                       {"hdr", m_hdr},
                       {"aspect", m_aspectRatio},
                       // Re-assert the panel's resolution on every (re)connect.
                       // Without this the phone silently reverted to its own
                       // 480p default each reconnect — the #1 "stream looks
                       // soft/noisy" cause (Zoom upscaling a 480p feed).
                       {"resWidth", RES_WIDTHS[m_resolutionIndex]},
                       {"resHeight", RES_HEIGHTS[m_resolutionIndex]}};
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
    if (controls.contains("lens")) {
        const bool front = controls.value("lens").toString() == QStringLiteral("front");
        if (front != m_lensFront) { m_lensFront = front; emit lensFrontChanged(); }
    }
    if (controls.contains("aspect")) {
        const QString v = controls.value("aspect").toString();
        if (!v.isEmpty() && v != m_aspectRatio) { m_aspectRatio = v; emit aspectRatioChanged(); }
    }
    if (controls.contains("zoomMax")) {
        const double v = controls.value("zoomMax").toDouble(m_zoomMax);
        if (v >= 1.0 && !qFuzzyCompare(v, m_zoomMax)) { m_zoomMax = v; emit zoomMaxChanged(); }
    }
    if (controls.contains("zoom")) {
        // Don't adopt a stale echo while a local wheel gesture is still pending
        // — the phone hasn't applied our newest value yet.
        const bool sendPending = m_zoomSendTimer && m_zoomSendTimer->isActive();
        const double v = controls.value("zoom").toDouble(m_zoom);
        if (!sendPending && v >= 1.0 && !qFuzzyCompare(v, m_zoom)) {
            m_zoom = v;
            emit zoomChanged();
        }
    }
}

void CameraControlViewModel::setResolution(int index) {
    if (index < 0 || index >= RES_COUNT || m_resolutionIndex == index) return;
    m_resolutionIndex = index;
    emit resolutionIndexChanged();
    emit controlPatch(QJsonObject{{"resWidth", RES_WIDTHS[index]}, {"resHeight", RES_HEIGHTS[index]}});
}

void CameraControlViewModel::flipLens() {
    m_lensFront = !m_lensFront;
    emit lensFrontChanged();
    emit controlPatch(QJsonObject{{"lens", m_lensFront ? QStringLiteral("front") : QStringLiteral("back")}});
}

void CameraControlViewModel::triggerFocus() {
    emit controlPatch(QJsonObject{{"focusTap", true}});
}

void CameraControlViewModel::setAspectRatio(const QString &ratio) {
    if (m_aspectRatio == ratio)
        return;
    m_aspectRatio = ratio;
    emit aspectRatioChanged();
    emit controlPatch(QJsonObject{{"aspect", ratio}});
}

void CameraControlViewModel::zoomBy(double factor) {
    if (factor <= 0.0)
        return;
    const double next = qBound(1.0, m_zoom * factor, m_zoomMax);
    if (qFuzzyCompare(next, m_zoom))
        return;
    m_zoom = next;
    emit zoomChanged();
    // Coalesce: a wheel burst updates the local value instantly (smooth UI) but
    // sends only the final ratio ~70ms after the last tick, so the phone gets
    // one CONTROL instead of one per detent.
    if (!m_zoomSendTimer) {
        m_zoomSendTimer = new QTimer(this);
        m_zoomSendTimer->setSingleShot(true);
        m_zoomSendTimer->setInterval(70);
        connect(m_zoomSendTimer, &QTimer::timeout, this, [this]() {
            emit controlPatch(QJsonObject{{"zoom", m_zoom}});
        });
    }
    m_zoomSendTimer->start();
}

void CameraControlViewModel::resetZoom() {
    if (qFuzzyCompare(m_zoom, 1.0))
        return;
    m_zoom = 1.0;
    emit zoomChanged();
    if (m_zoomSendTimer)
        m_zoomSendTimer->stop();
    emit controlPatch(QJsonObject{{"zoom", 1.0}});
}

void CameraControlViewModel::reset() {
    // Back to panel defaults without emitting controlPatch (we're disconnecting).
    if (m_torch) { m_torch = false; emit torchChanged(); }
    if (m_focusLock) { m_focusLock = false; emit focusLockChanged(); }
    if (!m_exposureLock) { m_exposureLock = true; emit exposureLockChanged(); }
    if (!m_hdr) { m_hdr = true; emit hdrChanged(); }
    if (!m_hdrSupported) { m_hdrSupported = true; emit hdrSupportedChanged(); }
    if (!m_torchAvailable) { m_torchAvailable = true; emit torchAvailableChanged(); }
    if (m_resolutionIndex != 1) { m_resolutionIndex = 1; emit resolutionIndexChanged(); }
    if (m_lensFront) { m_lensFront = false; emit lensFrontChanged(); }
    if (m_aspectRatio != QStringLiteral("full")) { m_aspectRatio = QStringLiteral("full"); emit aspectRatioChanged(); }
    if (m_zoomSendTimer) m_zoomSendTimer->stop();
    if (!qFuzzyCompare(m_zoom, 1.0)) { m_zoom = 1.0; emit zoomChanged(); }
    if (!qFuzzyCompare(m_zoomMax, 4.0)) { m_zoomMax = 4.0; emit zoomMaxChanged(); }
}
