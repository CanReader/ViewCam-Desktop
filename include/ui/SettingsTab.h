#pragma once

#include <QWidget>

class Settings;

class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(Settings *settings, QWidget *parent = nullptr);
};
