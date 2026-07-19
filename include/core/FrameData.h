#pragma once

#include <QByteArray>
#include <cstdint>

struct FrameData {
  // Compressed video payload: a complete JPEG (format 0) or one H264 Annex-B
  // access unit (format 1). Name kept from the MJPEG-only era — renaming
  // would touch every consumer for zero behavior change.
  QByteArray jpegData;
  // Header format byte: 0 = MJPEG, 1 = H264 (vc::FrameFormat values).
  uint8_t format = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint64_t timestamp = 0; // microseconds
  // The phone sends the sensor-oriented JPEG untouched (rotating on-device
  // cost it ~half its frame rate); the decoder rotates clockwise by this many
  // degrees after decode, then mirrors horizontally if requested (front cam).
  int rotationDegrees = 0; // 0 / 90 / 180 / 270
  bool mirror = false;
};
