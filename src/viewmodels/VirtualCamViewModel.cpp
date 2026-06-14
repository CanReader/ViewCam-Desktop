#include "viewmodels/VirtualCamViewModel.h"
#include "core/Logger.h"

VirtualCamViewModel::VirtualCamViewModel(QObject *parent) : QObject(parent) {}

void VirtualCamViewModel::setAvailable(bool available,
                                       const QString &devicePath) {
  if (m_available == available && m_devicePath == devicePath)
    return;
  m_available = available;
  m_devicePath = devicePath;
  emit availableChanged();
}

void VirtualCamViewModel::setEnabled(bool enabled) {
  if (m_enabled == enabled)
    return;
  m_enabled = enabled;
  VC_INFO("Virtual camera feed {}", enabled ? "enabled" : "disabled");
  emit enabledChanged();
}
