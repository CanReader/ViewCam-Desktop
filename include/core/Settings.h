#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

class Settings : public QObject {
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);

    QString lastHost() const;
    void setLastHost(const QString &host);

    int port() const;
    void setPort(int port);

    // Generic access for the QML settings layer
    QVariant value(const QString &key, const QVariant &defaultValue = {}) const;
    void setValue(const QString &key, const QVariant &value);

private:
    QSettings m_settings;
};
