#include "ViewCamConfig.h"
#include "core/Logger.h"
#include "core/QmlDevMode.h"
#include "viewmodels/AppController.h"

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QResource>
#include <QTranslator>

namespace {

// The whole UI (qml + qmldir + fonts/images/icons) and translations live in an
// external, opaque rcc next to the executable — nothing is embedded or loose.
bool registerResourceBundle() {
  const QString rcc =
      QGuiApplication::applicationDirPath() + QStringLiteral("/viewcam.rcc");
  if (!QResource::registerResource(rcc)) {
    VC_CRITICAL("Missing or unreadable resource bundle: {}", rcc.toStdString());
    VC_CRITICAL("ViewCam cannot start without viewcam.rcc next to the binary.");
    return false;
  }
  VC_INFO("Registered resource bundle: {}", rcc.toStdString());
  return true;
}

void loadFonts() {
  const char *fonts[] = {
      ":/qt/qml/ViewCam/Studio/resources/fonts/Geist-Regular.ttf",
      ":/qt/qml/ViewCam/Studio/resources/fonts/Geist-Medium.ttf",
      ":/qt/qml/ViewCam/Studio/resources/fonts/Geist-SemiBold.ttf",
      ":/qt/qml/ViewCam/Studio/resources/fonts/Geist-Bold.ttf",
      ":/qt/qml/ViewCam/Studio/resources/fonts/GeistMono-Regular.ttf",
      ":/qt/qml/ViewCam/Studio/resources/fonts/GeistMono-Medium.ttf",
  };
  for (const char *f : fonts) {
    if (QFontDatabase::addApplicationFont(QLatin1String(f)) < 0)
      VC_WARN("Failed to load font {}", f);
  }
}

// Loads viewcam_<locale>.qm from the bundle's qrc:/i18n directory. Adding a
// language is purely a matter of dropping a new .qm into the bundle.
void installTranslations(QGuiApplication &app, QTranslator &translator) {
  const QString locale = QLocale::system().name();
  if (translator.load(QStringLiteral("viewcam_") + locale,
                      QStringLiteral(":/i18n")) ||
      translator.load(QStringLiteral("viewcam_") + locale.left(2),
                      QStringLiteral(":/i18n"))) {
    app.installTranslator(&translator);
    VC_INFO("Loaded translation for locale: {}", locale.toStdString());
  } else {
    VC_INFO("No translation for locale {}, using source strings",
            locale.toStdString());
  }
}

} // namespace

int main(int argc, char *argv[]) {
  Logger::init(Logger::Level::Debug);
  VC_INFO("{} v{} starting", VIEWCAM_DISPLAY_NAME, VIEWCAM_VERSION_STRING);

  QGuiApplication app(argc, argv);
  // All identity from the generated config — drives QSettings + platform.
  app.setApplicationName(QStringLiteral(VIEWCAM_NAME));
  app.setApplicationDisplayName(QStringLiteral(VIEWCAM_DISPLAY_NAME));
  app.setApplicationVersion(QStringLiteral(VIEWCAM_VERSION_STRING));
  app.setOrganizationName(QStringLiteral(VIEWCAM_ORG_NAME));
  app.setOrganizationDomain(QStringLiteral(VIEWCAM_ORG_DOMAIN));

  if (!registerResourceBundle())
    return 2;

  app.setWindowIcon(
      QIcon(":/qt/qml/ViewCam/Studio/resources/images/monogram.svg"));

  loadFonts();
  QFont appFont(QStringLiteral("Geist"));
  appFont.setPixelSize(15);
  app.setFont(appFont);

  QTranslator translator;
  installTranslations(app, translator);

  // AppController is a QML_SINGLETON — the engine constructs and init()s it
  // lazily on first reference (Theme → AppController), so we don't build one
  // here. main() only owns the engine and process-level setup.
  QQmlApplicationEngine engine;
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
      []() {
        VC_CRITICAL("QML root object failed to load");
        QCoreApplication::exit(1);
      },
      Qt::QueuedConnection);

  // Dev mode (VIEWCAM_QML_DEV=1): load QML from on-disk source files and
  // hot-reload on save. No effect in normal / release use.
  QmlDevMode devMode;
  if (QmlDevMode::active())
    devMode.install(&engine);

  engine.loadFromModule("ViewCam.Studio", "Main");

  VC_INFO("Event loop starting");
  const int ret = app.exec();
  VC_INFO("Exiting with code {}", ret);
  Logger::shutdown();
  return ret;
}
