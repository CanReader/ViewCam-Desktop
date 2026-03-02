#include "ui/CameraPreviewWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setStyleSheet("background-color: #0D0D0D; border-radius: 10px;");
}

void CameraPreviewWidget::updateFrame(const QImage &image) {
    m_currentFrame = image;
    update();
}

void CameraPreviewWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Clip to rounded rect
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), 10, 10);
    painter.setClipPath(clipPath);

    if (m_currentFrame.isNull()) {
        // Modern "No signal" state
        painter.fillRect(rect(), QColor(0x12, 0x12, 0x18));

        int cx = width() / 2;
        int cy = height() / 2 - 20;

        // Outer ring
        painter.setPen(QPen(QColor(0x2A, 0x2D, 0x3A), 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(QPoint(cx, cy), 28, 28);

        // Camera body
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x2A, 0x2D, 0x3A));
        painter.drawRoundedRect(cx - 22, cy - 14, 44, 28, 6, 6);

        // Lens outer
        painter.setBrush(QColor(0x3D, 0x40, 0x55));
        painter.drawEllipse(QPoint(cx, cy), 11, 11);

        // Lens inner (dark)
        painter.setBrush(QColor(0x12, 0x12, 0x18));
        painter.drawEllipse(QPoint(cx, cy), 6, 6);

        // Lens highlight
        painter.setBrush(QColor(0x4D, 0x51, 0x65));
        painter.drawEllipse(QPoint(cx - 2, cy - 2), 2, 2);

        // "No Signal" text
        QFont font = painter.font();
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(0x6B, 0x6F, 0x82));
        painter.drawText(QRect(0, cy + 36, width(), 24), Qt::AlignHCenter, tr("No Signal"));

        // Subtitle
        font.setPixelSize(12);
        font.setWeight(QFont::Normal);
        painter.setFont(font);
        painter.setPen(QColor(0x4D, 0x51, 0x65));
        painter.drawText(QRect(0, cy + 56, width(), 20), Qt::AlignHCenter, tr("Waiting for connection\xe2\x80\xa6"));
        return;
    }

    QSize scaled = m_currentFrame.size().scaled(size(), Qt::KeepAspectRatio);
    QRect target(
        (width() - scaled.width()) / 2,
        (height() - scaled.height()) / 2,
        scaled.width(),
        scaled.height()
    );

    painter.fillRect(rect(), QColor(0x0D, 0x0D, 0x0D));
    painter.drawImage(target, m_currentFrame);
}
