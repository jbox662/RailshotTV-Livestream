#include "capture/ImageSource.h"

#include "core/Logger.h"

#include <QImage>

#include <algorithm>
#include <chrono>

namespace railshot {

namespace {

void imageToNv12(const QImage& image, VideoFrame& out) {
    QImage rgba = image.convertToFormat(QImage::Format_RGBA8888);
    const int w = rgba.width();
    const int h = rgba.height();
    out.allocate(w, h, PixelFormat::NV12);

    auto* yPlane = out.data.data();
    auto* uvPlane = out.data.data() + w * h;

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

} // namespace

ImageSource::ImageSource(Source config)
    : config_(std::move(config)) {}

ImageSource::~ImageSource() {
    stop();
}

bool ImageSource::loadImage() {
    if (config_.pathOrDeviceId.empty()) {
        Logger::error("ImageSource: no file path for " + config_.name);
        return false;
    }

    QImage image(QString::fromStdString(config_.pathOrDeviceId));
    if (image.isNull()) {
        Logger::error("ImageSource: failed to load " + config_.pathOrDeviceId);
        return false;
    }

    imageToNv12(image, frame_);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame_.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    return true;
}

bool ImageSource::start() {
    if (running_.load()) {
        return true;
    }
    if (!loadImage()) {
        return false;
    }
    running_ = true;
    return true;
}

void ImageSource::stop() {
    running_ = false;
}

bool ImageSource::isRunning() const {
    return running_.load();
}

std::optional<VideoFrame> ImageSource::latestVideoFrame() {
    std::lock_guard lock(mutex_);
    if (!running_.load() || !frame_.isValid()) {
        return std::nullopt;
    }
    return frame_;
}

std::optional<AudioFrame> ImageSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
