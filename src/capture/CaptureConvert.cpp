#include "capture/CaptureConvert.h"

#include <algorithm>
#include <cstring>

namespace railshot {

void qImageToNv12(const QImage& image, VideoFrame& out) {
    const QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const int w = rgba.width();
    const int h = rgba.height();
    out.allocate(w, h, PixelFormat::NV12);

    auto* yPlane = out.data.data();
    auto* uvPlane = out.data.data() + static_cast<size_t>(w) * static_cast<size_t>(h);

    for (int row = 0; row < h; ++row) {
        const auto* line = reinterpret_cast<const uchar*>(rgba.constScanLine(row));
        for (int col = 0; col < w; ++col) {
            const int r = line[col * 4 + 0];
            const int g = line[col * 4 + 1];
            const int b = line[col * 4 + 2];

            const int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[row * w + col] = static_cast<uint8_t>(std::clamp(y, 0, 255));

            if ((row & 1) == 0 && (col & 1) == 0) {
                const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                const int uvIndex = (row / 2) * w + col;
                uvPlane[uvIndex] = static_cast<uint8_t>(std::clamp(u, 0, 255));
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }
}

void qImageToRgba32(const QImage& image, VideoFrame& out) {
    // Match BrowserSource: keep Qt ARGB32 bytes for compositor alpha blending.
    const QImage argb = image.format() == QImage::Format_ARGB32
                            ? image
                            : image.convertToFormat(QImage::Format_ARGB32);
    const int w = argb.width();
    const int h = argb.height();
    out.allocate(w, h, PixelFormat::RGBA32);
    for (int row = 0; row < h; ++row) {
        std::memcpy(out.data.data() + static_cast<size_t>(row) * static_cast<size_t>(w) * 4,
                    argb.constScanLine(row), static_cast<size_t>(w) * 4);
    }
}

void bgraToNv12(const uint8_t* bgra, int width, int height, int stride, VideoFrame& out) {
    out.allocate(width, height, PixelFormat::NV12);
    auto* yPlane = out.data.data();
    auto* uvPlane = out.data.data() + static_cast<size_t>(width) * static_cast<size_t>(height);

    for (int row = 0; row < height; ++row) {
        const auto* line = bgra + static_cast<size_t>(row) * static_cast<size_t>(stride);
        for (int col = 0; col < width; ++col) {
            const int b = line[col * 4 + 0];
            const int g = line[col * 4 + 1];
            const int r = line[col * 4 + 2];

            const int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[row * width + col] = static_cast<uint8_t>(std::clamp(y, 0, 255));

            if ((row & 1) == 0 && (col & 1) == 0) {
                const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                const int uvIndex = (row / 2) * width + col;
                uvPlane[uvIndex] = static_cast<uint8_t>(std::clamp(u, 0, 255));
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }
}

} // namespace railshot
