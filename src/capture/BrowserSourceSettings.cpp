#include "capture/BrowserSourceSettings.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace railshot {

BrowserSourceSettings BrowserSourceSettings::defaults() {
    return {};
}

BrowserSourceSettings BrowserSourceSettings::fromSource(const Source& source) {
    if (!source.overlaySettings.empty()) {
        BrowserSourceSettings settings = fromJson(source.overlaySettings);
        if (settings.url.trimmed().isEmpty() && !source.pathOrDeviceId.empty()) {
            settings.url = QString::fromStdString(source.pathOrDeviceId);
        }
        return settings;
    }

    BrowserSourceSettings settings = defaults();
    if (!source.pathOrDeviceId.empty()) {
        settings.url = QString::fromStdString(source.pathOrDeviceId);
    } else if (source.transform.width > 8 && source.transform.height > 8
               && (source.transform.width != 1920.0f || source.transform.height != 110.0f)) {
        settings.width = static_cast<int>(source.transform.width);
        settings.height = static_cast<int>(source.transform.height);
    }
    return settings;
}

BrowserSourceSettings BrowserSourceSettings::fromJson(const std::string& json) {
    BrowserSourceSettings settings = defaults();
    if (json.empty()) {
        return settings;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (!doc.isObject()) {
        return settings;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("url"))) {
        settings.url = obj.value(QStringLiteral("url")).toString();
    }
    if (obj.contains(QStringLiteral("width"))) {
        settings.width = obj.value(QStringLiteral("width")).toInt(kDefaultWidth);
    }
    if (obj.contains(QStringLiteral("height"))) {
        settings.height = obj.value(QStringLiteral("height")).toInt(kDefaultHeight);
    }
    if (obj.contains(QStringLiteral("customCss"))) {
        settings.customCss = obj.value(QStringLiteral("customCss")).toString();
    }
    if (obj.contains(QStringLiteral("isLocalFile"))) {
        settings.isLocalFile = obj.value(QStringLiteral("isLocalFile")).toBool();
    }
    if (obj.contains(QStringLiteral("rerouteAudio"))) {
        settings.rerouteAudio = obj.value(QStringLiteral("rerouteAudio")).toBool();
    }
    if (obj.contains(QStringLiteral("fpsCustom"))) {
        settings.fpsCustom = obj.value(QStringLiteral("fpsCustom")).toBool();
    }
    if (obj.contains(QStringLiteral("fps"))) {
        settings.fps = obj.value(QStringLiteral("fps")).toInt(kDefaultFps);
    }
    if (obj.contains(QStringLiteral("shutdownWhenNotVisible"))) {
        settings.shutdownWhenNotVisible = obj.value(QStringLiteral("shutdownWhenNotVisible")).toBool();
    }
    if (obj.contains(QStringLiteral("refreshWhenActive"))) {
        settings.refreshWhenActive = obj.value(QStringLiteral("refreshWhenActive")).toBool();
    }
    if (obj.contains(QStringLiteral("pagePermissions"))) {
        settings.pagePermissions = static_cast<BrowserPagePermission>(
            obj.value(QStringLiteral("pagePermissions")).toInt(static_cast<int>(BrowserPagePermission::ReadApp)));
    }

    settings.clampDimensions();
    settings.fps = std::clamp(settings.fps, 1, 60);
    return settings;
}

std::string BrowserSourceSettings::toJson() const {
    QJsonObject obj;
    obj[QStringLiteral("url")] = url;
    obj[QStringLiteral("width")] = width;
    obj[QStringLiteral("height")] = height;
    obj[QStringLiteral("customCss")] = customCss;
    obj[QStringLiteral("isLocalFile")] = isLocalFile;
    obj[QStringLiteral("rerouteAudio")] = rerouteAudio;
    obj[QStringLiteral("fpsCustom")] = fpsCustom;
    obj[QStringLiteral("fps")] = fps;
    obj[QStringLiteral("shutdownWhenNotVisible")] = shutdownWhenNotVisible;
    obj[QStringLiteral("refreshWhenActive")] = refreshWhenActive;
    obj[QStringLiteral("pagePermissions")] = static_cast<int>(pagePermissions);
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

SourceTransform BrowserSourceSettings::defaultSceneTransform() const {
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    return {(1920.0f - w) * 0.5f, (1080.0f - h) * 0.5f, w, h};
}

int BrowserSourceSettings::captureIntervalMs() const {
    const int rate = fpsCustom ? fps : kDefaultFps;
    return std::max(16, 1000 / std::max(1, rate));
}

void BrowserSourceSettings::clampDimensions() {
    width = std::clamp(width, 8, 8192);
    height = std::clamp(height, 8, 8192);
}

void BrowserSourceSettings::applyConfigToSource(Source& source) const {
    source.pathOrDeviceId = url.toStdString();
    source.overlaySettings = toJson();
}

void BrowserSourceSettings::applySceneSizeToSource(Source& source) const {
    int sceneW = width;
    int sceneH = height;
    if (sceneW < 64) {
        sceneW = kDefaultWidth;
    }
    if (sceneH < 64) {
        sceneH = kDefaultHeight;
    }
    source.transform.width = static_cast<float>(sceneW);
    source.transform.height = static_cast<float>(sceneH);
}

void BrowserSourceSettings::applyToSource(Source& source) const {
    applyConfigToSource(source);
    applySceneSizeToSource(source);
}

} // namespace railshot
