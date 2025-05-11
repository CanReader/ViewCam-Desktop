#include <QApplication>
#include "core/Logger.h"
#include "core/Application.h"

int main(int argc, char *argv[]) {
    Logger::init(Logger::Level::Debug);
    VC_INFO("ViewCam Desktop v0.1.0 starting");

    QApplication app(argc, argv);
    app.setApplicationName("ViewCam");
    app.setOrganizationName("ViewCam");

    Application viewcam;
    viewcam.init();

    VC_INFO("Event loop starting");
    int ret = app.exec();
    VC_INFO("Exiting with code {}", ret);
    Logger::shutdown();
    return ret;
}
