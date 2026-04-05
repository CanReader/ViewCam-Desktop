#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class Settings;

class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(Settings *settings, QWidget *parent = nullptr);

    void setThemeIndex(int index);

#ifdef _WIN32
    void updateFilterStatus();
#endif

signals:
    void themeChanged(int index); // 0=Auto, 1=Dark, 2=Light

private:
    void updateThemeButton(int index, bool active);

    // Segmented theme picker buttons: Auto | Dark | Light
    QPushButton *m_themeButtons[3] = { nullptr, nullptr, nullptr };

#ifdef _WIN32
    QLabel      *m_filterStatusLabel = nullptr;
    QLabel      *m_filterPathLabel   = nullptr;
    QPushButton *m_installBtn        = nullptr;
    QPushButton *m_uninstallBtn      = nullptr;
#endif
};
