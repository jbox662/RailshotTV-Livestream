#include "capture/ColorSource.h"

#include "capture/CaptureConvert.h"

#include <QColor>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <chrono>

namespace railshot {

ColorSourceSettings ColorSourceSettings::fromSource(const Source& source) {
    ColorSourceSettings settings;
    if (source.overlaySettings.empty()) {
        if (source.transform.width > 8) {
            settings.width = static_cast<int>(source.transform.width);
        }
        if (source.transform.height > 8) {
            settings.height = static_cast<int>(source.transform.height);
        }
        return settings;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(source.overlaySettings));
    if (!doc.isObject()) {
        return settings;
    }
    const QJsonObject obj = doc.object();
    settings.width = obj.value(QStringLiteral("width")).toInt(settings.width);
    settings.height = obj.value(QStringLiteral("height")).toInt(settings.height);
    settings.color = static_cast<unsigned int>(obj.value(QStringLiteral("color")).toVariant().toULongLong());
    return settings;
}

std::string ColorSourceSettings::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("width")] = width;
    obj[QStringLiteral("height")] = height;
    obj[QStringLiteral("color")] = static_cast<double>(color);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

void ColorSourceSettings::applyToSource(Source& source) const {
    source.overlaySettings = toJson();
    source.transform.width = static_cast<float>(width);
    source.transform.height = static_cast<float>(height);
}

ColorSource::ColorSource(Source config)
    : config_(std::move(config)) {}

ColorSource::~ColorSource() {
    stop();
}

void ColorSource::regenerate() {
    const ColorSourceSettings settings = ColorSourceSettings::fromSource(config_);
    const int w = std::max(8, settings.width);
    const int h = std::max(8, settings.height);
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(QColor::fromRgba(settings.color));
    qImageToRgba32(image, frame_);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame_.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

bool ColorSource::start() {
    std::lock_guard lock(mutex_);
    regenerate();
    running_ = true;
    return frame_.isValid();
}

void ColorSource::stop() {
    running_ = false;
}

bool ColorSource::isRunning() const {
    return running_.load();
}

void ColorSource::updateConfig(const Source& source) {
    std::lock_guard lock(mutex_);
    config_ = source;
    if (running_.load()) {
        regenerate();
    }
}

std::optional<VideoFrame> ColorSource::latestVideoFrame() {
    std::lock_guard lock(mutex_);
    if (!running_.load() || !frame_.isValid()) {
        return std::nullopt;
    }
    return frame_;
}

std::optional<AudioFrame> ColorSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
