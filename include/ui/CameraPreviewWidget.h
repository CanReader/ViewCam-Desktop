#pragma once

#include <QWidget>
#include <QImage>

class CameraPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraPreviewWidget(QWidget *parent = nullptr);

public slots:
    void updateFrame(const QImage &image);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_currentFrame;
};
