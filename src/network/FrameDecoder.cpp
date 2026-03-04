#include "network/FrameDecoder.h"
#include "core/Logger.h"

FrameDecoder::FrameDecoder(QObject *parent)
    : QObject(parent)
{
}

void FrameDecoder::decode(const FrameData &frame) {
    QImage image;
    if (image.loadFromData(frame.jpegData, "JPEG")) {
        emit imageReady(image);
    } else {
        VC_WARN("Failed to decode JPEG frame ({} bytes, {}x{})",
                frame.jpegData.size(), frame.width, frame.height);
    }
}
