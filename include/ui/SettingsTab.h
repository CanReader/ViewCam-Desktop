#pragma once

#include <QWidget>
#include <QComboBox>

class QLabel;
class QPushButton;
class Settings;

class SettingsTab : public QWidget {
    Q_OBJECT

public:
    explicit SettingsTab(Settings *settings, QWidget *parent = nullptr);

    void setThemeIndex(int index);

#ifdef _WIN32
    // Refresh the filter status display from the registry
    void updateFilterStatus();
#endif

signals:
    void themeChanged(int index); // 0=Auto, 1=Dark, 2=Light

private:
    QComboBox *m_themeCombo;

#ifdef _WIN32
    QLabel      *m_filterStatusLabel = nullptr;
    QLabel      *m_filterPathLabel   = nullptr;
    QPushButton *m_installBtn        = nullptr;
    QPushButton *m_uninstallBtn      = nullptr;
#endif
};
