#include "capture/TextSource.h"

#include "capture/CaptureConvert.h"

#include <QFont>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <algorithm>
#include <chrono>

namespace railshot {

TextSourceSettings TextSourceSettings::fromSource(const Source& source) {
    TextSourceSettings settings;
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
    settings.text = obj.value(QStringLiteral("text")).toString(settings.text);
    settings.fontFamily = obj.value(QStringLiteral("fontFamily")).toString(settings.fontFamily);
    settings.fontSize = obj.value(QStringLiteral("fontSize")).toInt(settings.fontSize);
    settings.color = static_cast<unsigned int>(obj.value(QStringLiteral("color")).toVariant().toULongLong());
    settings.width = obj.value(QStringLiteral("width")).toInt(settings.width);
    settings.height = obj.value(QStringLiteral("height")).toInt(settings.height);
    return settings;
}

std::string TextSourceSettings::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("text")] = text;
    obj[QStringLiteral("fontFamily")] = fontFamily;
    obj[QStringLiteral("fontSize")] = fontSize;
    obj[QStringLiteral("color")] = static_cast<double>(color);
    obj[QStringLiteral("width")] = width;
    obj[QStringLiteral("height")] = height;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

void TextSourceSettings::applyToSource(Source& source) const {
    source.overlaySettings = toJson();
    source.transform.width = static_cast<float>(width);
    source.transform.height = static_cast<float>(height);
}

TextSource::TextSource(Source config)
    : config_(std::move(config)) {}

TextSource::~TextSource() {
    stop();
}

void TextSource::regenerate() {
    const TextSourceSettings settings = TextSourceSettings::fromSource(config_);
    const int w = std::max(8, settings.width);
    const int h = std::max(8, settings.height);
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    QFont font(settings.fontFamily);
    font.setPixelSize(std::max(8, settings.fontSize));
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor::fromRgba(settings.color));
    painter.drawText(image.rect().adjusted(16, 8, -16, -8),
                     Qt::AlignCenter | Qt::TextWordWrap, settings.text);
    painter.end();

    qImageToRgba32(image, frame_);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame_.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

bool TextSource::start() {
    std::lock_guard lock(mutex_);
    regenerate();
    running_ = true;
    return frame_.isValid();
}

void TextSource::stop() {
    running_ = false;
}

bool TextSource::isRunning() const {
    return running_.load();
}

void TextSource::updateConfig(const Source& source) {
    std::lock_guard lock(mutex_);
    config_ = source;
    if (running_.load()) {
        regenerate();
    }
}

std::optional<VideoFrame> TextSource::latestVideoFrame() {
    std::lock_guard lock(mutex_);
    if (!running_.load() || !frame_.isValid()) {
        return std::nullopt;
    }
    return frame_;
}

std::optional<AudioFrame> TextSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
