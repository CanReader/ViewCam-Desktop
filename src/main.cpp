#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include "core/Logger.h"
#include "core/Application.h"

int main(int argc, char *argv[]) {
    Logger::init(Logger::Level::Debug);
    VC_INFO("ViewCam Desktop v0.1.0 starting");

    QApplication app(argc, argv);
    app.setApplicationName("ViewCam");
    app.setOrganizationName("ViewCam");

    QTranslator translator;
    QString translationsDir = QApplication::applicationDirPath() + "/translations";
    QString locale = QLocale::system().name();
    if (translator.load("viewcam_" + locale, translationsDir) ||
        translator.load("viewcam_" + locale.left(2), translationsDir)) {
        app.installTranslator(&translator);
        VC_INFO("Loaded translation for locale: {}", locale.toStdString());
    } else {
        VC_INFO("No translation found for locale: {}, using default", locale.toStdString());
    }

    Application viewcam;
    viewcam.init();

    VC_INFO("Event loop starting");
    int ret = app.exec();
    VC_INFO("Exiting with code {}", ret);
    Logger::shutdown();
    return ret;
}
