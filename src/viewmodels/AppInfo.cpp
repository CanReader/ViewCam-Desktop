#include "viewmodels/AppInfo.h"
#include "ViewCamConfig.h"

#include <QtGlobal>

QString AppInfo::name() const { return QStringLiteral(VIEWCAM_NAME); }
QString AppInfo::displayName() const { return QStringLiteral(VIEWCAM_DISPLAY_NAME); }
QString AppInfo::edition() const { return QStringLiteral(VIEWCAM_EDITION); }

QString AppInfo::editionLabel() const {
  // "Studio v1.0" — edition + short version, composed once here
  return QStringLiteral(VIEWCAM_EDITION " " VIEWCAM_SHORT_VERSION);
}

QString AppInfo::organization() const { return QStringLiteral(VIEWCAM_ORG_NAME); }
QString AppInfo::domain() const { return QStringLiteral(VIEWCAM_ORG_DOMAIN); }
QString AppInfo::description() const { return QStringLiteral(VIEWCAM_DESCRIPTION); }
QString AppInfo::homepage() const { return QStringLiteral(VIEWCAM_HOMEPAGE); }
QString AppInfo::versionString() const { return QStringLiteral(VIEWCAM_VERSION_STRING); }
QString AppInfo::shortVersion() const { return QStringLiteral(VIEWCAM_SHORT_VERSION); }
int AppInfo::versionMajor() const { return VIEWCAM_VERSION_MAJOR; }
int AppInfo::versionMinor() const { return VIEWCAM_VERSION_MINOR; }
int AppInfo::versionPatch() const { return VIEWCAM_VERSION_PATCH; }
int AppInfo::buildNumber() const { return VIEWCAM_BUILD_NUMBER; }
QString AppInfo::bundleId() const { return QStringLiteral(VIEWCAM_BUNDLE_ID); }
QString AppInfo::copyright() const { return QStringLiteral(VIEWCAM_COPYRIGHT); }
QString AppInfo::qtVersion() const { return QStringLiteral(VIEWCAM_QT_VERSION); }
QString AppInfo::qtRuntimeVersion() const { return QString::fromLatin1(qVersion()); }
QString AppInfo::buildType() const { return QStringLiteral(VIEWCAM_BUILD_TYPE); }
QString AppInfo::gitSha() const { return QStringLiteral(VIEWCAM_GIT_SHA); }
