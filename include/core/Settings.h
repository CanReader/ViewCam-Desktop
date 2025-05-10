#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class Settings : public QObject {
    Q_OBJECT

public:
    explicit Settings(QObject *parent = nullptr);

    QString lastHost() const;
    void setLastHost(const QString &host);

    int port() const;
    void setPort(int port);

private:
    QSettings m_settings;
};
