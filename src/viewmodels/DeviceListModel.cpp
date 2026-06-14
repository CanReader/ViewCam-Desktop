#include "viewmodels/DeviceListModel.h"
#include "core/Logger.h"
#include <QDateTime>

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent) {
  m_pruneTimer.setInterval(1000);
  connect(&m_pruneTimer, &QTimer::timeout, this, &DeviceListModel::prune);
  m_pruneTimer.start();
}

int DeviceListModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_devices.size())
    return {};
  const Device &d = m_devices[index.row()];
  switch (role) {
  case IdRole:
    return d.deviceId;
  case NameRole:
    return d.name;
  case HostRole:
    return d.host;
  case PortRole:
    return d.port;
  }
  return {};
}

QHash<int, QByteArray> DeviceListModel::roleNames() const {
  return {{IdRole, "deviceId"},
          {NameRole, "name"},
          {HostRole, "host"},
          {PortRole, "port"}};
}

void DeviceListModel::addOrUpdate(const QString &deviceId, const QString &name,
                                  const QString &host, int port) {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (int i = 0; i < m_devices.size(); ++i) {
    if (m_devices[i].deviceId == deviceId) {
      m_devices[i].lastSeenMs = now;
      if (m_devices[i].name != name || m_devices[i].host != host ||
          m_devices[i].port != port) {
        m_devices[i].name = name;
        m_devices[i].host = host;
        m_devices[i].port = port;
        emit dataChanged(index(i), index(i));
      }
      return;
    }
  }
  beginInsertRows({}, m_devices.size(), m_devices.size());
  m_devices.append({deviceId, name, host, port, now});
  endInsertRows();
  emit countChanged();
  VC_INFO("Device discovered: {} ({}) at {}:{}", name.toStdString(),
          deviceId.toStdString(), host.toStdString(), port);
}

void DeviceListModel::prune() {
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  for (int i = m_devices.size() - 1; i >= 0; --i) {
    if (now - m_devices[i].lastSeenMs > STALE_MS) {
      VC_DEBUG("Pruning stale device {}", m_devices[i].name.toStdString());
      beginRemoveRows({}, i, i);
      m_devices.removeAt(i);
      endRemoveRows();
      emit countChanged();
    }
  }
}
