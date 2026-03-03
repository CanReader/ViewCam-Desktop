#include "ui/CameraPreviewWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <cmath>

CameraPreviewWidget::CameraPreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Preferred);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
}

void CameraPreviewWidget::updateFrame(const QImage &image) {
    m_currentFrame = image;
    if (!image.isNull() && image.height() > 0) {
        double newRatio = static_cast<double>(image.width()) / image.height();
        if (std::abs(newRatio - m_aspectRatio) > 0.01) {
            m_aspectRatio = newRatio;
            updateGeometry();
        }
    }
    update();
}

bool CameraPreviewWidget::hasHeightForWidth() const {
    return true;
}

int CameraPreviewWidget::heightForWidth(int w) const {
    return static_cast<int>(w / m_aspectRatio);
}

void CameraPreviewWidget::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Clip to rounded rect
    QPainterPath clipPath;
    clipPath.addRoundedRect(rect(), 14, 14);
    painter.setClipPath(clipPath);

    if (m_currentFrame.isNull()) {
        // Dark "No signal" state
        painter.fillRect(rect(), QColor(0x06, 0x0a, 0x14));

        int cx = width() / 2;
        int cy = height() / 2 - 20;

        // Camera body
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0x17, 0x20, 0x33));
        painter.drawRoundedRect(cx - 22, cy - 14, 44, 28, 6, 6);

        // Lens outer
        painter.setBrush(QColor(0x1e, 0x29, 0x3b));
        painter.drawEllipse(QPoint(cx, cy), 11, 11);

        // Lens inner
        painter.setBrush(QColor(0x06, 0x0a, 0x14));
        painter.drawEllipse(QPoint(cx, cy), 6, 6);

        // Lens highlight
        painter.setBrush(QColor(0x4f, 0xc3, 0xf7, 80));
        painter.drawEllipse(QPoint(cx - 2, cy - 2), 2, 2);

        // "No Signal" text
        QFont font = painter.font();
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(0x7a, 0x8b, 0xaa));
        painter.drawText(QRect(0, cy + 36, width(), 24), Qt::AlignHCenter, tr("No Signal"));

        // Subtitle
        font.setPixelSize(12);
        font.setWeight(QFont::Normal);
        painter.setFont(font);
        painter.setPen(QColor(0x47, 0x55, 0x69));
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

    painter.fillRect(rect(), QColor(0x00, 0x00, 0x00));
    painter.drawImage(target, m_currentFrame);
}
