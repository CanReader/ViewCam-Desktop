#pragma once

#include <QWidget>
#include <QImage>

class CameraPreviewWidget : public QWidget {
    Q_OBJECT

public:
    explicit CameraPreviewWidget(QWidget *parent = nullptr);

public slots:
    void updateFrame(const QImage &image);

    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_currentFrame;
    double m_aspectRatio = 16.0 / 9.0;
};
