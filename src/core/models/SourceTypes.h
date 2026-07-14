#pragma once

#include "core/FrameData.h"

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
    ColorCorrection,
    Gain,
    Compressor,
    NoiseGate,
    NoiseSuppress
};

struct SourceFilter {
    std::string id;
    FilterType type = FilterType::Opacity;
    bool enabled = true;
    // Opacity / ColorCorrection / Gain / Compressor / NoiseGate / NoiseSuppress JSON
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
    int syncDelayMs = 0; // OBS-like audio sync offset (ms), delayed into the mix
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

// Apply enabled audio filters in order onto an S16 PCM frame (in place).
void applyAudioFilters(const Source& source, AudioFrame& frame);

} // namespace railshot
