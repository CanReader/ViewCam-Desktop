#include "core/Settings.h"
#include "core/Constants.h"

Settings::Settings(QObject *parent)
    : QObject(parent)
    // Default ctor picks up QCoreApplication's org/app name (set in main from
    // the generated config) — single source of truth for the settings path.
    , m_settings()
{
}

QString Settings::lastHost() const {
    return m_settings.value("connection/host", "").toString();
}

void Settings::setLastHost(const QString &host) {
    m_settings.setValue("connection/host", host);
}

int Settings::port() const {
    return m_settings.value("connection/lastPort", vc::kStreamPort).toInt();
}

void Settings::setPort(int port) {
    // "connection/lastPort", NOT "connection/port": the latter backs the
    // user's "Listen port" preference (SettingsViewModel::listenPort), and
    // writing it here on every connect silently overwrote that preference
    // with whatever port the last device advertised — with no change signal,
    // so the Settings page also displayed a stale value.
    m_settings.setValue("connection/lastPort", port);
}

QVariant Settings::value(const QString &key, const QVariant &defaultValue) const {
    return m_settings.value(key, defaultValue);
}

void Settings::setValue(const QString &key, const QVariant &value) {
    m_settings.setValue(key, value);
}
