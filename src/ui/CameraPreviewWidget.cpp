#include "ui/CameraPreviewWidget.h"
#include <QPainter>

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: black;");
}

void CameraPreviewWidget::updateFrame(const QImage &image) {
    m_currentFrame = image;
    update();
}

void CameraPreviewWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_currentFrame.isNull()) {
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, "No signal");
        return;
    }

    QSize scaled = m_currentFrame.size().scaled(size(), Qt::KeepAspectRatio);
    QRect target(
        (width() - scaled.width()) / 2,
        (height() - scaled.height()) / 2,
        scaled.width(),
        scaled.height()
    );

    painter.fillRect(rect(), Qt::black);
    painter.drawImage(target, m_currentFrame);
}
