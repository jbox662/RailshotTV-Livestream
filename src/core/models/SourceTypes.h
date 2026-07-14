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
    NoiseSuppress,
    Crop,
    ChromaKey,
    ColorKey,
    ImageMask,
    ColorGrade,
    Scale,
    Scroll,
    Sharpness,
    RenderDelay
};

struct SourceFilter {
    std::string id;
    FilterType type = FilterType::Opacity;
    bool enabled = true;
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

    std::string pathOrDeviceId;

    int volume = 100;
    bool muted = false;
    int syncDelayMs = 0;
    bool loop = true;
    bool isoRecording = false;

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
    float brightness = 0.0f;
    float contrast = 1.0f;
    float saturation = 1.0f;

    // Relative crop insets 0..1
    float cropLeft = 0.0f;
    float cropTop = 0.0f;
    float cropRight = 0.0f;
    float cropBottom = 0.0f;

    float scale = 1.0f; // UV zoom (1 = identity)

    float scrollSpeedX = 0.0f; // UV units / second
    float scrollSpeedY = 0.0f;
    bool scrollLoop = true;

    float sharpness = 0.0f; // 0..1

    int keyMode = 0; // 0 none, 1 color key, 2 chroma key
    float keyR = 0.0f;
    float keyG = 1.0f;
    float keyB = 0.0f;
    float keySimilarity = 0.4f; // 0..1
    float keySmoothness = 0.08f;
    float keySpill = 0.1f;

    float gradeAmount = 0.0f;
    float lift = 0.0f;
    float gamma = 1.0f;
    float gain = 1.0f;

    bool maskEnabled = false;
    float maskOpacity = 1.0f;
    std::string maskPath;

    float lutAmount = 0.0f;
    std::string lutPath;

    int renderDelayMs = 0;
};

FilterRenderParams resolveFilters(const Source& source);

void applyAudioFilters(const Source& source, AudioFrame& frame);

} // namespace railshot
