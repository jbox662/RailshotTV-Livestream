#include "capture/MediaSourceSettings.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace railshot {

MediaSourceSettings MediaSourceSettings::defaults() {
    return {};
}

MediaSourceSettings MediaSourceSettings::fromSource(const Source& source) {
    MediaSourceSettings settings = defaults();
    if (!source.overlaySettings.empty()) {
        settings = fromJson(source.overlaySettings);
    }
    if (settings.localFile.isEmpty() && !source.pathOrDeviceId.empty()) {
        settings.localFile = QString::fromStdString(source.pathOrDeviceId);
        settings.isLocalFile = true;
    }
    settings.looping = source.loop;
    return settings;
}

MediaSourceSettings MediaSourceSettings::fromJson(const std::string& json) {
    MediaSourceSettings settings = defaults();
    if (json.empty()) {
        return settings;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject()) {
        return settings;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("localFile"))) {
        settings.localFile = obj.value(QStringLiteral("localFile")).toString();
    }
    if (obj.contains(QStringLiteral("isLocalFile"))) {
        settings.isLocalFile = obj.value(QStringLiteral("isLocalFile")).toBool(true);
    }
    if (obj.contains(QStringLiteral("looping"))) {
        settings.looping = obj.value(QStringLiteral("looping")).toBool(true);
    }
    if (obj.contains(QStringLiteral("restartOnActivate"))) {
        settings.restartOnActivate = obj.value(QStringLiteral("restartOnActivate")).toBool(true);
    }
    if (obj.contains(QStringLiteral("hardwareDecode"))) {
        settings.hardwareDecode = obj.value(QStringLiteral("hardwareDecode")).toBool(true);
    }
    if (obj.contains(QStringLiteral("speedPercent"))) {
        settings.speedPercent = obj.value(QStringLiteral("speedPercent")).toDouble(100.0);
    } else if (obj.contains(QStringLiteral("speed"))) {
        settings.speedPercent = obj.value(QStringLiteral("speed")).toDouble(1.0) * 100.0;
    }
    settings.speedPercent = std::clamp(settings.speedPercent, 1.0, 200.0);
    return settings;
}

std::string MediaSourceSettings::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("localFile")] = localFile;
    obj[QStringLiteral("isLocalFile")] = isLocalFile;
    obj[QStringLiteral("looping")] = looping;
    obj[QStringLiteral("restartOnActivate")] = restartOnActivate;
    obj[QStringLiteral("hardwareDecode")] = hardwareDecode;
    obj[QStringLiteral("speedPercent")] = speedPercent;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

double MediaSourceSettings::playbackRate() const {
    return std::clamp(speedPercent, 1.0, 200.0) / 100.0;
}

void MediaSourceSettings::applyToSource(Source& source) const {
    source.pathOrDeviceId = localFile.trimmed().toStdString();
    source.loop = looping;
    source.overlaySettings = toJson();
}

} // namespace railshot
