#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// State of the virtual camera backend (v4l2loopback / DirectShow).
// `enabled` is the user-facing toggle; `available` reflects whether the
// backend device could be opened at all.
class VirtualCamViewModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by AppController")
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString devicePath READ devicePath NOTIFY availableChanged)

public:
    explicit VirtualCamViewModel(QObject *parent = nullptr);

    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    QString devicePath() const { return m_devicePath; }

    void setAvailable(bool available, const QString &devicePath = {});
    void setEnabled(bool enabled);

signals:
    void availableChanged();
    void enabledChanged();

private:
    bool m_available = false;
    bool m_enabled = false;
    QString m_devicePath;
};
