#include "network/FrameDecoder.h"
#include "core/Logger.h"

#include <QTransform>

FrameDecoder::FrameDecoder(QObject *parent)
    : QObject(parent)
{
}

void FrameDecoder::decode(const FrameData &frame) {
    QImage image;
    if (image.loadFromData(frame.jpegData, "JPEG")) {
        // Sensor-to-upright transform (header byte 22). Applied once here so
        // every consumer (preview + virtual camera) gets the corrected image.
        // Right-angle rotations are lossless pixel remaps, and doing this on
        // the desktop roughly doubled the phone's achievable frame rate.
        if (frame.rotationDegrees != 0) {
            QTransform t;
            t.rotate(frame.rotationDegrees);
            image = image.transformed(t, Qt::FastTransformation);
        }
        if (frame.mirror) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
            image = image.flipped(Qt::Horizontal);
#else
            image = image.mirrored(true, false); // flipped() only exists in 6.9+
#endif
        }
        emit imageReady(image);
    } else {
        VC_WARN("Failed to decode JPEG frame ({} bytes, {}x{})",
                frame.jpegData.size(), frame.width, frame.height);
    }
}
