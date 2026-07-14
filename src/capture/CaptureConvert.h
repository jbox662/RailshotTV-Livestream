#pragma once

// Shared QImage -> NV12 helper for static / software-rendered sources.
#include "core/FrameData.h"

#include <QImage>

namespace railshot {

void qImageToNv12(const QImage& image, VideoFrame& out);
void qImageToRgba32(const QImage& image, VideoFrame& out);
void bgraToNv12(const uint8_t* bgra, int width, int height, int stride, VideoFrame& out);

} // namespace railshot
