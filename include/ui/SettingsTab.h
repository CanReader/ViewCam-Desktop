#pragma once

#include <QWidget>
#include <QComboBox>

class Settings;

class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(Settings *settings, QWidget *parent = nullptr);

    void setThemeIndex(int index);

signals:
    void themeChanged(int index); // 0=Auto, 1=Dark, 2=Light

private:
    QComboBox *m_themeCombo;
};
