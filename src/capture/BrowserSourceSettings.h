#pragma once

#include "core/models/SourceTypes.h"

#include <QString>

namespace railshot {

// Browser page permission level for embedded overlays.
enum class BrowserPagePermission : int {
    None = 0,
    ReadApp = 1,
    ReadUser = 2,
    Basic = 3,
    Advanced = 4,
    All = 5
};

struct BrowserSourceSettings {
    static constexpr int kDefaultWidth = 800;
    static constexpr int kDefaultHeight = 600;
    static constexpr int kDefaultFps = 30;
    static constexpr const char* kDefaultUrl = "about:blank";
    static constexpr const char* kDefaultCss =
        "body { background-color: rgba(0, 0, 0, 0); margin: 0px auto; overflow: hidden; }";

    QString url = QString::fromUtf8(kDefaultUrl);
    int width = kDefaultWidth;
    int height = kDefaultHeight;
    QString customCss = QString::fromUtf8(kDefaultCss);
    bool isLocalFile = false;

    bool rerouteAudio = false;
    bool fpsCustom = false;
    int fps = kDefaultFps;
    bool shutdownWhenNotVisible = false;
    bool refreshWhenActive = false;
    BrowserPagePermission pagePermissions = BrowserPagePermission::ReadApp;

    [[nodiscard]] static BrowserSourceSettings defaults();
    [[nodiscard]] static BrowserSourceSettings fromSource(const Source& source);
    [[nodiscard]] static BrowserSourceSettings fromJson(const std::string& json);
    [[nodiscard]] std::string toJson() const;
    [[nodiscard]] SourceTransform defaultSceneTransform() const;
    [[nodiscard]] int captureIntervalMs() const;

    void clampDimensions();
    // Writes URL / overlay JSON / browser render size. Does not resize the scene item.
    void applyConfigToSource(Source& source) const;
    // Also sets the scene item box to Width × Height (keeps x/y).
    void applyToSource(Source& source) const;
    void applySceneSizeToSource(Source& source) const;
};

} // namespace railshot
