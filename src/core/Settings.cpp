#include "core/Settings.h"

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_settings("ViewCam", "ViewCam")
{
}

QString Settings::lastHost() const {
    return m_settings.value("connection/host", "").toString();
}

void Settings::setLastHost(const QString &host) {
    m_settings.setValue("connection/host", host);
}

int Settings::port() const {
    return m_settings.value("connection/port", 8080).toInt();
}

void Settings::setPort(int port) {
    m_settings.setValue("connection/port", port);
}
