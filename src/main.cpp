#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "ViewCamConfig.h"
#include "core/Logger.h"
#include "core/QmlDevMode.h"
#include "updater/UpdateChecker.h"
#include "viewmodels/AppController.h"
#include "viewmodels/SettingsViewModel.h"

#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLocale>
#include <QQmlApplicationEngine>
#include <QResource>
#include <QSettings>
#include <QTimer>
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

// (Re)load viewcam_<locale>.qm from :/i18n. Empty langCode → system locale.
// Safe to call at runtime: removes the old translator before installing the new one.
void loadTranslation(QGuiApplication &app, QTranslator &t, const QString &langCode) {
    app.removeTranslator(&t);
    const QString locale = langCode.isEmpty() ? QLocale::system().name() : langCode;
    if (t.load(QStringLiteral("viewcam_") + locale, QStringLiteral(":/i18n")) ||
        t.load(QStringLiteral("viewcam_") + locale.left(2), QStringLiteral(":/i18n"))) {
        app.installTranslator(&t);
        VC_INFO("Loaded translation for locale: {}", locale.toStdString());
    } else {
        VC_INFO("No translation for locale {}, using source strings",
                locale.toStdString());
    }
}

// Initial load: reads the user-selected language from QSettings (written by
// SettingsViewModel) so the correct translation is applied before any QML loads.
void installTranslations(QGuiApplication &app, QTranslator &translator) {
    QSettings s;
    const QString saved =
        s.value(QStringLiteral("appearance/language"), QString{}).toString();
    loadTranslation(app, translator, saved);
}

} // namespace

int main(int argc, char *argv[]) {
#if defined(_WIN32) && defined(NDEBUG)
  // GUI subsystem: no console on double-click, but reattach stdout when the
  // user launches from a terminal so log output still reaches them.
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
      FILE *fp = nullptr;
      freopen_s(&fp, "CONOUT$", "w", stdout);
      freopen_s(&fp, "CONOUT$", "w", stderr);
  }
  Logger::init(Logger::Level::Info);
#else
  Logger::init(Logger::Level::Debug);
#endif
  VC_INFO("{} v{} starting", VIEWCAM_DISPLAY_NAME, VIEWCAM_VERSION_STRING);

  int ret;
  {
    QGuiApplication app(argc, argv);
    // All identity from the generated config — drives QSettings + platform.
    app.setApplicationName(QStringLiteral(VIEWCAM_NAME));
    app.setApplicationDisplayName(QStringLiteral(VIEWCAM_DISPLAY_NAME));
    app.setApplicationVersion(QStringLiteral(VIEWCAM_VERSION_STRING));
    app.setOrganizationName(QStringLiteral(VIEWCAM_ORG_NAME));
    app.setOrganizationDomain(QStringLiteral(VIEWCAM_ORG_DOMAIN));

    // Post-update first-run: if relaunched by vc-updater with --just-updated,
    // clear the .pending-verify sentinel so the helper/launcher knows this
    // version booted successfully (no rollback needed).
    UpdateChecker::clearPendingVerifyIfJustUpdated(app.arguments());

    if (!registerResourceBundle())
      return 2;

    app.setWindowIcon(
        QIcon(":/qt/qml/ViewCam/Studio/resources/images/Logo.svg"));

    loadFonts();
    QFont appFont(QStringLiteral("Geist"));
    appFont.setPixelSize(15);
    app.setFont(appFont);

    QTranslator translator;
    installTranslations(app, translator);

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() {
          VC_CRITICAL("QML root object failed to load");
          QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);

    // Eagerly create AppController before QML loads so the language-changed
    // connection below is guaranteed to fire when the user switches languages,
    // regardless of when QML first references the singleton.
    AppController *controller = AppController::create(&engine, nullptr);
    {
        auto *vm = controller->settings();
        QObject::connect(vm, &SettingsViewModel::languageChanged, &app,
                         [&, vm]() {
                             loadTranslation(app, translator, vm->language());
                             engine.retranslate();
                         });
    }

    // Dev mode (VIEWCAM_QML_DEV=1): load QML from on-disk source files and
    // hot-reload on save. No effect in normal / release use.
    QmlDevMode devMode;
    if (QmlDevMode::active())
      devMode.install(&engine);

    // Self-update (Phase 5 UX): the CHECK no longer blocks the splash. Run a
    // background check shortly after the app is interactive (the periodic timer
    // and manual "Check for updates" live in QML / Settings). The splash only
    // takes over for an actual apply-at-launch moment.
    UpdateChecker *updater = UpdateChecker::instance();
    QTimer::singleShot(3000, updater, [updater]() { updater->start(); });

    // Shutdown hook: if the user chose "Install on quit" and the download
    // verified, apply the staged update on exit (helper swaps + relaunches).
    QObject::connect(&app, &QGuiApplication::aboutToQuit, updater,
                     [updater]() { updater->applyPendingOnQuit(); });

    engine.loadFromModule("ViewCam.Studio", "Main");

    VC_INFO("Event loop starting");
    ret = app.exec();
    VC_INFO("Exiting with code {}", ret);
  } // engine and app (and all their children) destroyed here
  Logger::shutdown(); // safe: no Qt objects alive
  return ret;
}
