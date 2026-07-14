#pragma once

#include "core/ProductionProfile.h"

#include <mutex>
#include <string>

namespace railshot {

struct HotkeyBindings {
    std::string transition = "T";
    std::string startStopStream = "F9";
    std::string record = "F10";
    std::string scene1 = "1";
    std::string scene2 = "2";
    std::string scene3 = "3";
    std::string scene4 = "4";
    std::string scoreP1Plus = "]";
    std::string scoreP1Minus = "[";
    std::string scoreP2Plus = "'";
    std::string scoreP2Minus = ";";
    std::string saveReplay = "F12";
};

struct AppSettingsData {
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    int fps = 30;
    std::string defaultRtmpUrl;
    HotkeyBindings hotkeys;
    int productionProfile = 0; // ProductionProfile as int
    std::string activeCollectionId;

    // Audio (Phase F2)
    bool audioMonitoringEnabled = true;
    std::string monitoringDeviceId; // empty = default render
    std::string micDeviceId;        // empty = default capture
    int micVolume = 100;
    bool micMuted = false;
    int micSyncDelayMs = 0;

    // Output / encoder (Phase F5)
    // videoEncoder: auto | h264_nvenc | h264_qsv | h264_amf | libx264
    std::string videoEncoder = "auto";
    std::string encoderPreset = "default"; // codec-specific; "default" maps per encoder
    int videoBitrateKbps = 6000;
    int audioBitrateKbps = 160;
    std::string recordingFormat = "mp4"; // mp4 | mkv | mov
    std::string recordingDirectory;      // empty = Movies/RailShot/Recordings
    int recordingBitrateKbps = 0;        // 0 = same as stream bitrate
    bool replayBufferEnabled = false;
    int replayBufferSeconds = 30;
    std::string streamService = "Custom"; // Custom | Twitch | YouTube | Kick | Facebook
    std::string streamServer;
    std::string streamKey;

    // Remote protocol (Phase F7) — empty = no password (local tools)
    std::string websocketPassword;
};

class AppSettings {
public:
    static AppSettings& instance();

    void load();
    bool save() const;

    [[nodiscard]] AppSettingsData data() const;
    void setData(const AppSettingsData& data);

    [[nodiscard]] int canvasWidth() const;
    [[nodiscard]] int canvasHeight() const;
    [[nodiscard]] int fps() const;
    [[nodiscard]] std::string defaultRtmpUrl() const;
    [[nodiscard]] HotkeyBindings hotkeys() const;
    [[nodiscard]] ProductionProfile productionProfile() const;
    [[nodiscard]] std::string activeCollectionId() const;

    void setDefaultRtmpUrl(const std::string& url);
    void setActiveCollectionId(const std::string& id);

    void setMicVolume(int volume);
    void setMicMuted(bool muted);
    void setMicSyncDelayMs(int ms);
    void setAudioMonitoringEnabled(bool enabled);

private:
    AppSettings();

    mutable std::mutex mutex_;
    AppSettingsData data_;
    std::string savePath_;
};

} // namespace railshot
