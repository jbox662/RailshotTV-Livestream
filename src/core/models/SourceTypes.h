#pragma once

#include <string>
#include <vector>

namespace railshot {

enum class SourceType {
    VideoDevice,
    AudioDevice,
    Image,
    MediaFile,
    NDI,
    Browser,
    Scoreboard,
    DisplayCapture,
    WindowCapture,
    Text,
    Color,
    DesktopAudio
};

enum class TransitionType {
    Cut,
    Fade,
    Slide,
    FadeToBlack
};

enum class FilterType {
    Opacity,
    ColorCorrection
};

struct SourceFilter {
    std::string id;
    FilterType type = FilterType::Opacity;
    bool enabled = true;
    // Opacity: {"opacity":0..1}  ColorCorrection: {"brightness":-1..1,"contrast":0..2,"saturation":0..2}
    std::string paramsJson;
};

struct SourceTransform {
    float x = 0.0f;
    float y = 0.0f;
    float width = 1920.0f;
    float height = 1080.0f;
    float rotation = 0.0f;
    float cropLeft = 0.0f;
    float cropTop = 0.0f;
    float cropRight = 0.0f;
    float cropBottom = 0.0f;
};

struct Source {
    std::string id;
    std::string name;
    SourceType type = SourceType::VideoDevice;
    SourceTransform transform;
    bool isVisible = true;
    bool locked = false;
    int zOrder = 0;

    // VideoDevice / AudioDevice device id, or file path for Image / MediaFile
    std::string pathOrDeviceId;

    int volume = 100;
    bool muted = false;
    bool loop = true;
    bool isoRecording = false;

    // JSON settings for overlay sources (scoreboard style, browser options, etc.)
    std::string overlaySettings;

    std::vector<SourceFilter> filters;
};

struct Scene {
    std::string id;
    std::string name;
    std::vector<Source> sources;
    TransitionType transitionType = TransitionType::Fade;
};

struct SceneCollection {
    std::string id = "default";
    std::string name = "Default Collection";
    std::vector<Scene> scenes;
};

struct CollectionInfo {
    std::string id;
    std::string name;
};

struct FilterRenderParams {
    float opacity = 1.0f;
    float brightness = 0.0f;  // -1..1
    float contrast = 1.0f;    // 0..2
    float saturation = 1.0f;  // 0..2
};

FilterRenderParams resolveFilters(const Source& source);

} // namespace railshot
