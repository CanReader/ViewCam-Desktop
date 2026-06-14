#pragma once

#include <QAbstractListModel>
#include <QTimer>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include "core/Constants.h"

// Discovered phones, fed by DeviceDiscovery beacons and keyed by the stable
// per-install deviceId. Cards that stop beaconing are pruned after the
// protocol's card timeout so the list reflects reality.
class DeviceListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
    };

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_devices.size(); }

    void addOrUpdate(const QString &deviceId, const QString &name,
                     const QString &host, int port);

signals:
    void countChanged();

private:
    void prune();

    struct Device {
        QString deviceId;
        QString name;
        QString host;
        int port = 0;
        qint64 lastSeenMs = 0;
    };

    QVector<Device> m_devices;
    QTimer m_pruneTimer;

    static constexpr qint64 STALE_MS = vc::kCardTimeoutMs;
};
