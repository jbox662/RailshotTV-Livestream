#include "core/AppSettings.h"

#include "core/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>
#include <fstream>

namespace railshot {

namespace {

QJsonObject hotkeysToJson(const HotkeyBindings& h) {
    QJsonObject obj;
    obj[QStringLiteral("transition")] = QString::fromStdString(h.transition);
    obj[QStringLiteral("startStopStream")] = QString::fromStdString(h.startStopStream);
    obj[QStringLiteral("record")] = QString::fromStdString(h.record);
    obj[QStringLiteral("scene1")] = QString::fromStdString(h.scene1);
    obj[QStringLiteral("scene2")] = QString::fromStdString(h.scene2);
    obj[QStringLiteral("scene3")] = QString::fromStdString(h.scene3);
    obj[QStringLiteral("scene4")] = QString::fromStdString(h.scene4);
    obj[QStringLiteral("scoreP1Plus")] = QString::fromStdString(h.scoreP1Plus);
    obj[QStringLiteral("scoreP1Minus")] = QString::fromStdString(h.scoreP1Minus);
    obj[QStringLiteral("scoreP2Plus")] = QString::fromStdString(h.scoreP2Plus);
    obj[QStringLiteral("scoreP2Minus")] = QString::fromStdString(h.scoreP2Minus);
    obj[QStringLiteral("saveReplay")] = QString::fromStdString(h.saveReplay);
    return obj;
}

HotkeyBindings hotkeysFromJson(const QJsonObject& obj) {
    HotkeyBindings defaults;
    HotkeyBindings h = defaults;
    auto read = [&](const char* key, std::string& target) {
        if (obj.contains(QLatin1String(key))) {
            target = obj.value(QLatin1String(key)).toString().toStdString();
        }
    };
    read("transition", h.transition);
    read("startStopStream", h.startStopStream);
    read("record", h.record);
    read("scene1", h.scene1);
    read("scene2", h.scene2);
    read("scene3", h.scene3);
    read("scene4", h.scene4);
    read("scoreP1Plus", h.scoreP1Plus);
    read("scoreP1Minus", h.scoreP1Minus);
    read("scoreP2Plus", h.scoreP2Plus);
    read("scoreP2Minus", h.scoreP2Minus);
    read("saveReplay", h.saveReplay);
    return h;
}

} // namespace

AppSettings& AppSettings::instance() {
    static AppSettings inst;
    return inst;
}

AppSettings::AppSettings() {
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    savePath_ = (dataDir + "/settings.json").toStdString();
    load();
}

void AppSettings::load() {
    std::lock_guard lock(mutex_);
    std::ifstream in(savePath_);
    if (!in) {
        return;
    }
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(content));
    if (!doc.isObject()) {
        return;
    }
    const QJsonObject obj = doc.object();
    data_.canvasWidth = obj.value(QStringLiteral("canvasWidth")).toInt(1920);
    data_.canvasHeight = obj.value(QStringLiteral("canvasHeight")).toInt(1080);
    data_.fps = obj.value(QStringLiteral("fps")).toInt(30);
    data_.defaultRtmpUrl = obj.value(QStringLiteral("defaultRtmpUrl")).toString().toStdString();
    data_.productionProfile = obj.value(QStringLiteral("productionProfile")).toInt(0);
    data_.activeCollectionId =
        obj.value(QStringLiteral("activeCollectionId")).toString().toStdString();
    // Legacy: collectionDisplayName was a shadow rename; ignore on load going forward.
    if (obj.value(QStringLiteral("hotkeys")).isObject()) {
        data_.hotkeys = hotkeysFromJson(obj.value(QStringLiteral("hotkeys")).toObject());
    }
    data_.audioMonitoringEnabled = obj.value(QStringLiteral("audioMonitoringEnabled")).toBool(true);
    data_.monitoringDeviceId =
        obj.value(QStringLiteral("monitoringDeviceId")).toString().toStdString();
    data_.micDeviceId = obj.value(QStringLiteral("micDeviceId")).toString().toStdString();
    data_.micVolume = obj.value(QStringLiteral("micVolume")).toInt(100);
    data_.micMuted = obj.value(QStringLiteral("micMuted")).toBool(false);
    data_.micSyncDelayMs = obj.value(QStringLiteral("micSyncDelayMs")).toInt(0);

    data_.videoEncoder = obj.value(QStringLiteral("videoEncoder")).toString(QStringLiteral("auto")).toStdString();
    data_.encoderPreset =
        obj.value(QStringLiteral("encoderPreset")).toString(QStringLiteral("default")).toStdString();
    data_.videoBitrateKbps = obj.value(QStringLiteral("videoBitrateKbps")).toInt(6000);
    data_.audioBitrateKbps = obj.value(QStringLiteral("audioBitrateKbps")).toInt(160);
    data_.recordingFormat =
        obj.value(QStringLiteral("recordingFormat")).toString(QStringLiteral("mp4")).toStdString();
    data_.recordingDirectory =
        obj.value(QStringLiteral("recordingDirectory")).toString().toStdString();
    data_.recordingBitrateKbps = obj.value(QStringLiteral("recordingBitrateKbps")).toInt(0);
    data_.replayBufferEnabled = obj.value(QStringLiteral("replayBufferEnabled")).toBool(false);
    data_.replayBufferSeconds = obj.value(QStringLiteral("replayBufferSeconds")).toInt(30);
    data_.streamService =
        obj.value(QStringLiteral("streamService")).toString(QStringLiteral("Custom")).toStdString();
    data_.streamServer = obj.value(QStringLiteral("streamServer")).toString().toStdString();
    data_.streamKey = obj.value(QStringLiteral("streamKey")).toString().toStdString();

    data_.canvasWidth = std::clamp(data_.canvasWidth, 320, 7680);
    data_.canvasHeight = std::clamp(data_.canvasHeight, 240, 4320);
    data_.fps = std::clamp(data_.fps, 15, 60);
    data_.micVolume = std::clamp(data_.micVolume, 0, 100);
    data_.micSyncDelayMs = std::clamp(data_.micSyncDelayMs, 0, 2000);
    data_.videoBitrateKbps = std::clamp(data_.videoBitrateKbps, 500, 100000);
    data_.audioBitrateKbps = std::clamp(data_.audioBitrateKbps, 64, 512);
    data_.recordingBitrateKbps = std::clamp(data_.recordingBitrateKbps, 0, 100000);
    data_.replayBufferSeconds = std::clamp(data_.replayBufferSeconds, 5, 300);
    if (data_.recordingFormat != "mkv" && data_.recordingFormat != "mov") {
        data_.recordingFormat = "mp4";
    }
}

bool AppSettings::save() const {
    std::lock_guard lock(mutex_);
    QJsonObject obj;
    obj[QStringLiteral("canvasWidth")] = data_.canvasWidth;
    obj[QStringLiteral("canvasHeight")] = data_.canvasHeight;
    obj[QStringLiteral("fps")] = data_.fps;
    obj[QStringLiteral("defaultRtmpUrl")] = QString::fromStdString(data_.defaultRtmpUrl);
    obj[QStringLiteral("productionProfile")] = data_.productionProfile;
    obj[QStringLiteral("activeCollectionId")] = QString::fromStdString(data_.activeCollectionId);
    obj[QStringLiteral("hotkeys")] = hotkeysToJson(data_.hotkeys);
    obj[QStringLiteral("audioMonitoringEnabled")] = data_.audioMonitoringEnabled;
    obj[QStringLiteral("monitoringDeviceId")] = QString::fromStdString(data_.monitoringDeviceId);
    obj[QStringLiteral("micDeviceId")] = QString::fromStdString(data_.micDeviceId);
    obj[QStringLiteral("micVolume")] = data_.micVolume;
    obj[QStringLiteral("micMuted")] = data_.micMuted;
    obj[QStringLiteral("micSyncDelayMs")] = data_.micSyncDelayMs;
    obj[QStringLiteral("videoEncoder")] = QString::fromStdString(data_.videoEncoder);
    obj[QStringLiteral("encoderPreset")] = QString::fromStdString(data_.encoderPreset);
    obj[QStringLiteral("videoBitrateKbps")] = data_.videoBitrateKbps;
    obj[QStringLiteral("audioBitrateKbps")] = data_.audioBitrateKbps;
    obj[QStringLiteral("recordingFormat")] = QString::fromStdString(data_.recordingFormat);
    obj[QStringLiteral("recordingDirectory")] = QString::fromStdString(data_.recordingDirectory);
    obj[QStringLiteral("recordingBitrateKbps")] = data_.recordingBitrateKbps;
    obj[QStringLiteral("replayBufferEnabled")] = data_.replayBufferEnabled;
    obj[QStringLiteral("replayBufferSeconds")] = data_.replayBufferSeconds;
    obj[QStringLiteral("streamService")] = QString::fromStdString(data_.streamService);
    obj[QStringLiteral("streamServer")] = QString::fromStdString(data_.streamServer);
    obj[QStringLiteral("streamKey")] = QString::fromStdString(data_.streamKey);

    const QByteArray bytes = QJsonDocument(obj).toJson(QJsonDocument::Indented);
    std::ofstream out(savePath_, std::ios::binary | std::ios::trunc);
    if (!out) {
        Logger::warn("AppSettings: failed to write " + savePath_);
        return false;
    }
    out.write(bytes.constData(), bytes.size());
    return true;
}

AppSettingsData AppSettings::data() const {
    std::lock_guard lock(mutex_);
    return data_;
}

void AppSettings::setData(const AppSettingsData& data) {
    {
        std::lock_guard lock(mutex_);
        data_ = data;
        data_.canvasWidth = std::clamp(data_.canvasWidth, 320, 7680);
        data_.canvasHeight = std::clamp(data_.canvasHeight, 240, 4320);
        data_.fps = std::clamp(data_.fps, 15, 60);
        data_.micVolume = std::clamp(data_.micVolume, 0, 100);
        data_.micSyncDelayMs = std::clamp(data_.micSyncDelayMs, 0, 2000);
        data_.videoBitrateKbps = std::clamp(data_.videoBitrateKbps, 500, 100000);
        data_.audioBitrateKbps = std::clamp(data_.audioBitrateKbps, 64, 512);
        data_.recordingBitrateKbps = std::clamp(data_.recordingBitrateKbps, 0, 100000);
        data_.replayBufferSeconds = std::clamp(data_.replayBufferSeconds, 5, 300);
        if (data_.recordingFormat != "mkv" && data_.recordingFormat != "mov") {
            data_.recordingFormat = "mp4";
        }
    }
    save();
}

int AppSettings::canvasWidth() const {
    std::lock_guard lock(mutex_);
    return data_.canvasWidth;
}

int AppSettings::canvasHeight() const {
    std::lock_guard lock(mutex_);
    return data_.canvasHeight;
}

int AppSettings::fps() const {
    std::lock_guard lock(mutex_);
    return data_.fps;
}

std::string AppSettings::defaultRtmpUrl() const {
    std::lock_guard lock(mutex_);
    return data_.defaultRtmpUrl;
}

HotkeyBindings AppSettings::hotkeys() const {
    std::lock_guard lock(mutex_);
    return data_.hotkeys;
}

ProductionProfile AppSettings::productionProfile() const {
    std::lock_guard lock(mutex_);
    return static_cast<ProductionProfile>(data_.productionProfile);
}

void AppSettings::setDefaultRtmpUrl(const std::string& url) {
    {
        std::lock_guard lock(mutex_);
        data_.defaultRtmpUrl = url;
    }
    save();
}

std::string AppSettings::activeCollectionId() const {
    std::lock_guard lock(mutex_);
    return data_.activeCollectionId;
}

void AppSettings::setActiveCollectionId(const std::string& id) {
    {
        std::lock_guard lock(mutex_);
        data_.activeCollectionId = id;
    }
    save();
}

void AppSettings::setMicVolume(int volume) {
    {
        std::lock_guard lock(mutex_);
        data_.micVolume = std::clamp(volume, 0, 100);
    }
    save();
}

void AppSettings::setMicMuted(bool muted) {
    {
        std::lock_guard lock(mutex_);
        data_.micMuted = muted;
    }
    save();
}

void AppSettings::setMicSyncDelayMs(int ms) {
    {
        std::lock_guard lock(mutex_);
        data_.micSyncDelayMs = std::clamp(ms, 0, 2000);
    }
    save();
}

void AppSettings::setAudioMonitoringEnabled(bool enabled) {
    {
        std::lock_guard lock(mutex_);
        data_.audioMonitoringEnabled = enabled;
    }
    save();
}

} // namespace railshot
