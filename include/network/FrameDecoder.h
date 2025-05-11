#pragma once

#include <QObject>
#include <QImage>
#include "core/FrameData.h"

class FrameDecoder : public QObject {
    Q_OBJECT

public:
    explicit FrameDecoder(QObject *parent = nullptr);

public slots:
    void decode(const FrameData &frame);

signals:
    void imageReady(const QImage &image);
};
