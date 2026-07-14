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
};

struct AppSettingsData {
    int canvasWidth = 1920;
    int canvasHeight = 1080;
    int fps = 30;
    std::string defaultRtmpUrl;
    HotkeyBindings hotkeys;
    int productionProfile = 0; // ProductionProfile as int
    std::string activeCollectionId;
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

private:
    AppSettings();

    mutable std::mutex mutex_;
    AppSettingsData data_;
    std::string savePath_;
};

} // namespace railshot
