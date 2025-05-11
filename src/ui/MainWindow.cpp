#include "ui/MainWindow.h"
#include "ui/CameraPreviewWidget.h"
#include "ui/ConnectionPanel.h"
#include "core/Logger.h"
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("ViewCam");
    resize(800, 600);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_preview = new CameraPreviewWidget(central);
    m_connectionPanel = new ConnectionPanel(central);

    layout->addWidget(m_preview, 1);
    layout->addWidget(m_connectionPanel, 0);

    setCentralWidget(central);
    VC_DEBUG("MainWindow created (800x600)");
}
