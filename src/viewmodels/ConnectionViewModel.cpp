#include "viewmodels/ConnectionViewModel.h"
#include "core/Logger.h"

ConnectionViewModel::ConnectionViewModel(QObject *parent) : QObject(parent) {
  m_statsTimer.setInterval(1000);
  connect(&m_statsTimer, &QTimer::timeout, this, [this]() {
    m_fps = m_framesThisSecond;
    m_bitrateMbps = (m_bytesThisSecond * 8.0) / 1e6;
    m_frameIntervalMs =
        m_intervalSamples > 0 ? m_intervalAccum / m_intervalSamples : 0;
    m_framesThisSecond = 0;
    m_bytesThisSecond = 0;
    m_intervalAccum = 0;
    m_intervalSamples = 0;
    emit statsChanged();
  });

  m_uptimeTimer.setInterval(1000);
  connect(&m_uptimeTimer, &QTimer::timeout, this,
          [this]() { emit uptimeChanged(); });
}

int ConnectionViewModel::linkQuality() const {
  // 0 = Terrible .. 3 = Strong. Derived from inter-frame pacing while
  // streaming; a freshly-connected idle link (no frames yet) reads as Strong.
  if (m_state != Connected)
    return 0;
  if (m_frameIntervalMs <= 0.0)
    return 3; // connected, no video yet — link itself is healthy
  if (m_frameIntervalMs <= 38.0)
    return 3; // ~26 fps and up
  if (m_frameIntervalMs <= 60.0)
    return 2; // ~16-26 fps
  if (m_frameIntervalMs <= 120.0)
    return 1;
  return 0;
}

QString ConnectionViewModel::linkQualityLabel() const {
  if (m_state != Connected)
    return QStringLiteral("—");
  switch (linkQuality()) {
  case 3:
    return QStringLiteral("Strong");
  case 2:
    return QStringLiteral("Fine");
  case 1:
    return QStringLiteral("Low");
  default:
    return QStringLiteral("Terrible");
  }
}

QString ConnectionViewModel::uptimeText() const {
  if (m_state != Connected)
    return QStringLiteral("00:00:00");
  const qint64 secs = m_uptime.elapsed() / 1000;
  return QStringLiteral("%1:%2:%3")
      .arg(secs / 3600, 2, 10, QLatin1Char('0'))
      .arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
      .arg(secs % 60, 2, 10, QLatin1Char('0'));
}

void ConnectionViewModel::beginConnecting(const QString &name,
                                          const QString &host, int port) {
  m_deviceName = name;
  m_deviceOs.clear();
  m_lens.clear();
  m_host = host;
  m_port = port;
  m_errorText.clear();
  m_sessionLimited = false;
  m_battery = -1;
  m_charging = false;
  emit deviceChanged();
  emit batteryChanged();
  emit errorChanged();
  emit sessionLimitedChanged();
  setState(Connecting);
}

void ConnectionViewModel::setHelloInfo(const QString &name, const QString &os) {
  if (!name.isEmpty())
    m_deviceName = name; // HELLO name is authoritative over the beacon label
  m_deviceOs = os;
  emit deviceChanged();
}

void ConnectionViewModel::setLens(const QString &lens) {
  if (m_lens == lens)
    return;
  m_lens = lens;
  emit deviceChanged();
}

void ConnectionViewModel::setPowerStatus(int batteryPercent, bool charging) {
  const int clamped =
      (batteryPercent < 0 || batteryPercent > 100) ? -1 : batteryPercent;
  if (m_battery == clamped && m_charging == charging)
    return;
  m_battery = clamped;
  m_charging = charging;
  emit batteryChanged();
}

void ConnectionViewModel::markConnected() {
  m_uptime.start();
  m_lastFrameTime.invalidate();
  resetStats();
  m_statsTimer.start();
  m_uptimeTimer.start();
  setState(Connected);
  emit uptimeChanged();
  // link quality/label depend on state but notify via statsChanged — refresh
  // them now so the UI doesn't wait for the first 1s stats tick.
  emit statsChanged();
}

void ConnectionViewModel::markDisconnected() {
  m_statsTimer.stop();
  m_uptimeTimer.stop();
  resetStats();
  emit statsChanged();
  setState(Disconnected);
}

void ConnectionViewModel::markError(const QString &error) {
  m_errorText = error;
  emit errorChanged();
  markDisconnected();
}

void ConnectionViewModel::setSessionLimited(bool limited) {
  if (m_sessionLimited == limited)
    return;
  m_sessionLimited = limited;
  emit sessionLimitedChanged();
}

void ConnectionViewModel::onFrame(const FrameData &frame) {
  m_framesThisSecond++;
  m_bytesThisSecond += frame.jpegData.size();
  if (m_lastFrameTime.isValid()) {
    m_intervalAccum += m_lastFrameTime.nsecsElapsed() / 1e6;
    m_intervalSamples++;
  }
  m_lastFrameTime.start();
  if (frame.width != static_cast<uint32_t>(m_frameWidth) ||
      frame.height != static_cast<uint32_t>(m_frameHeight)) {
    m_frameWidth = frame.width;
    m_frameHeight = frame.height;
    emit statsChanged();
  }
}

void ConnectionViewModel::setState(State s) {
  if (m_state == s)
    return;
  m_state = s;
  emit stateChanged();
}

void ConnectionViewModel::resetStats() {
  m_fps = 0;
  m_bitrateMbps = 0;
  m_frameIntervalMs = 0;
  m_framesThisSecond = 0;
  m_bytesThisSecond = 0;
  m_intervalAccum = 0;
  m_intervalSamples = 0;
  m_frameWidth = 0;
  m_frameHeight = 0;
}
