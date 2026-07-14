#include "core/models/SourceTypes.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace railshot {
namespace {

struct GateState {
    bool open = false;
    float holdRemainingMs = 0.0f;
    float envelope = 0.0f;
};

struct CompState {
    float envelope = 0.0f;
};

std::mutex& audioFilterMutex() {
    static std::mutex m;
    return m;
}

std::unordered_map<std::string, GateState>& gateStates() {
    static std::unordered_map<std::string, GateState> s;
    return s;
}

std::unordered_map<std::string, CompState>& compStates() {
    static std::unordered_map<std::string, CompState> s;
    return s;
}

float frameRms(const int16_t* samples, size_t count) {
    if (count == 0) {
        return 0.0f;
    }
    double sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const double n = static_cast<double>(samples[i]) / 32768.0;
        sum += n * n;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

float linearToDb(float linear) {
    const float v = std::max(linear, 1.0e-8f);
    return 20.0f * std::log10(v);
}

float dbToGain(float db) {
    return std::pow(10.0f, db / 20.0f);
}

void applyGainDb(AudioFrame& frame, float db) {
    const float gain = dbToGain(db);
    auto* samples = reinterpret_cast<int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    for (size_t i = 0; i < count; ++i) {
        const float scaled = static_cast<float>(samples[i]) * gain;
        samples[i] = static_cast<int16_t>(std::clamp(static_cast<int>(scaled), -32768, 32767));
    }
}

void applyCompressor(const std::string& key, AudioFrame& frame, float ratio, float thresholdDb,
                     float attackMs, float releaseMs, float outputGainDb) {
    auto* samples = reinterpret_cast<int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    if (count == 0 || frame.channels <= 0 || frame.sampleRate <= 0) {
        return;
    }

    CompState state;
    {
        std::lock_guard lock(audioFilterMutex());
        state = compStates()[key];
    }

    const float attackCoeff =
        std::exp(-1.0f / std::max(1.0f, attackMs * 0.001f * static_cast<float>(frame.sampleRate)));
    const float releaseCoeff =
        std::exp(-1.0f / std::max(1.0f, releaseMs * 0.001f * static_cast<float>(frame.sampleRate)));
    const float outGain = dbToGain(outputGainDb);
    const float safeRatio = std::max(1.0f, ratio);

    for (size_t i = 0; i < count; i += static_cast<size_t>(frame.channels)) {
        float peak = 0.0f;
        for (int c = 0; c < frame.channels; ++c) {
            peak = std::max(peak, std::abs(static_cast<float>(samples[i + c]) / 32768.0f));
        }
        if (peak > state.envelope) {
            state.envelope = attackCoeff * state.envelope + (1.0f - attackCoeff) * peak;
        } else {
            state.envelope = releaseCoeff * state.envelope + (1.0f - releaseCoeff) * peak;
        }

        const float envDb = linearToDb(state.envelope);
        float gainDb = 0.0f;
        if (envDb > thresholdDb) {
            gainDb = thresholdDb + (envDb - thresholdDb) / safeRatio - envDb;
        }
        const float gain = dbToGain(gainDb) * outGain;
        for (int c = 0; c < frame.channels; ++c) {
            const float scaled = static_cast<float>(samples[i + c]) * gain;
            samples[i + c] =
                static_cast<int16_t>(std::clamp(static_cast<int>(scaled), -32768, 32767));
        }
    }

    std::lock_guard lock(audioFilterMutex());
    compStates()[key] = state;
}

void applyNoiseGate(const std::string& key, AudioFrame& frame, float openDb, float closeDb,
                    float attackMs, float holdMs, float releaseMs) {
    auto* samples = reinterpret_cast<int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    if (count == 0 || frame.sampleRate <= 0) {
        return;
    }

    GateState state;
    {
        std::lock_guard lock(audioFilterMutex());
        state = gateStates()[key];
    }

    const float rms = frameRms(samples, count);
    const float levelDb = linearToDb(rms);
    const float frameMs =
        1000.0f * static_cast<float>(count / std::max(1, frame.channels))
        / static_cast<float>(frame.sampleRate);

    if (state.open) {
        if (levelDb < closeDb) {
            state.holdRemainingMs -= frameMs;
            if (state.holdRemainingMs <= 0.0f) {
                state.open = false;
            }
        } else {
            state.holdRemainingMs = holdMs;
        }
    } else if (levelDb >= openDb) {
        state.open = true;
        state.holdRemainingMs = holdMs;
    }

    const float target = state.open ? 1.0f : 0.0f;
    const float coeffMs = state.open ? std::max(1.0f, attackMs) : std::max(1.0f, releaseMs);
    const float alpha = 1.0f - std::exp(-frameMs / coeffMs);
    state.envelope += (target - state.envelope) * alpha;

    for (size_t i = 0; i < count; ++i) {
        const float scaled = static_cast<float>(samples[i]) * state.envelope;
        samples[i] = static_cast<int16_t>(std::clamp(static_cast<int>(scaled), -32768, 32767));
    }

    std::lock_guard lock(audioFilterMutex());
    gateStates()[key] = state;
}

void applyNoiseSuppress(AudioFrame& frame, float suppressDb) {
    // Lightweight spectral-free suppressor: soft-expand quiet content toward silence.
    auto* samples = reinterpret_cast<int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    if (count == 0) {
        return;
    }
    const float rms = frameRms(samples, count);
    const float levelDb = linearToDb(rms);
    const float floorDb = -std::abs(suppressDb);
    if (levelDb >= floorDb) {
        return;
    }
    const float t = std::clamp((levelDb - (floorDb - 12.0f)) / 12.0f, 0.0f, 1.0f);
    const float gain = t * t;
    for (size_t i = 0; i < count; ++i) {
        const float scaled = static_cast<float>(samples[i]) * gain;
        samples[i] = static_cast<int16_t>(std::clamp(static_cast<int>(scaled), -32768, 32767));
    }
}

} // namespace

FilterRenderParams resolveFilters(const Source& source) {
    FilterRenderParams params;

    // Scene-item crop (pixels) → relative UV insets.
    if (source.transform.width > 1.0f && source.transform.height > 1.0f) {
        params.cropLeft = std::max(0.0f, source.transform.cropLeft / source.transform.width);
        params.cropTop = std::max(0.0f, source.transform.cropTop / source.transform.height);
        params.cropRight = std::max(0.0f, source.transform.cropRight / source.transform.width);
        params.cropBottom = std::max(0.0f, source.transform.cropBottom / source.transform.height);
    }

    auto parseKeyRgb = [](const QJsonObject& obj, float& r, float& g, float& b) {
        const QString type = obj.value(QStringLiteral("key_color_type")).toString(QStringLiteral("green"));
        if (type == QLatin1String("blue")) {
            r = 0.0f;
            g = 0.0f;
            b = 1.0f;
        } else if (type == QLatin1String("magenta")) {
            r = 1.0f;
            g = 0.0f;
            b = 1.0f;
        } else if (type == QLatin1String("red")) {
            r = 1.0f;
            g = 0.0f;
            b = 0.0f;
        } else if (type == QLatin1String("custom") || obj.contains(QStringLiteral("key_color"))) {
            const QJsonValue v = obj.value(QStringLiteral("key_color"));
            unsigned color = 0x00FF00u;
            if (v.isString()) {
                QString hex = v.toString().trimmed();
                if (hex.startsWith(QLatin1Char('#'))) {
                    hex = hex.mid(1);
                }
                bool ok = false;
                color = hex.toUInt(&ok, 16);
                if (!ok) {
                    color = 0x00FF00u;
                }
            } else {
                color = static_cast<unsigned>(v.toInt(0x00FF00));
            }
            r = static_cast<float>((color >> 16) & 0xFF) / 255.0f;
            g = static_cast<float>((color >> 8) & 0xFF) / 255.0f;
            b = static_cast<float>(color & 0xFF) / 255.0f;
        } else {
            r = 0.0f;
            g = 1.0f;
            b = 0.0f;
        }
    };

    for (const auto& filter : source.filters) {
        if (!filter.enabled) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
        switch (filter.type) {
        case FilterType::Opacity: {
            const float opacity =
                static_cast<float>(obj.value(QStringLiteral("opacity")).toDouble(1.0));
            params.opacity *= std::clamp(opacity, 0.0f, 1.0f);
            break;
        }
        case FilterType::ColorCorrection:
            params.brightness =
                static_cast<float>(obj.value(QStringLiteral("brightness")).toDouble(0.0));
            params.contrast =
                static_cast<float>(obj.value(QStringLiteral("contrast")).toDouble(1.0));
            params.saturation =
                static_cast<float>(obj.value(QStringLiteral("saturation")).toDouble(1.0));
            params.brightness = std::clamp(params.brightness, -1.0f, 1.0f);
            params.contrast = std::clamp(params.contrast, 0.0f, 2.0f);
            params.saturation = std::clamp(params.saturation, 0.0f, 2.0f);
            break;
        case FilterType::Crop: {
            const bool relative = obj.value(QStringLiteral("relative")).toBool(true);
            float l = static_cast<float>(obj.value(QStringLiteral("left")).toDouble(0.0));
            float t = static_cast<float>(obj.value(QStringLiteral("top")).toDouble(0.0));
            float r = static_cast<float>(obj.value(QStringLiteral("right")).toDouble(0.0));
            float b = static_cast<float>(obj.value(QStringLiteral("bottom")).toDouble(0.0));
            if (!relative && source.transform.width > 1.0f && source.transform.height > 1.0f) {
                l /= source.transform.width;
                t /= source.transform.height;
                r /= source.transform.width;
                b /= source.transform.height;
            }
            params.cropLeft = std::clamp(params.cropLeft + l, 0.0f, 0.49f);
            params.cropTop = std::clamp(params.cropTop + t, 0.0f, 0.49f);
            params.cropRight = std::clamp(params.cropRight + r, 0.0f, 0.49f);
            params.cropBottom = std::clamp(params.cropBottom + b, 0.0f, 0.49f);
            break;
        }
        case FilterType::Scale: {
            const float sx = static_cast<float>(obj.value(QStringLiteral("scale")).toDouble(1.0));
            params.scale = std::clamp(sx, 0.05f, 8.0f);
            break;
        }
        case FilterType::Scroll:
            params.scrollSpeedX =
                static_cast<float>(obj.value(QStringLiteral("speed_x")).toDouble(0.0));
            params.scrollSpeedY =
                static_cast<float>(obj.value(QStringLiteral("speed_y")).toDouble(0.0));
            params.scrollLoop = obj.value(QStringLiteral("loop")).toBool(true);
            break;
        case FilterType::Sharpness:
            params.sharpness = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("sharpness")).toDouble(0.08)), 0.0f,
                1.0f);
            break;
        case FilterType::ColorKey:
            params.keyMode = 1;
            parseKeyRgb(obj, params.keyR, params.keyG, params.keyB);
            params.keySimilarity = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("similarity")).toDouble(80.0)) / 1000.0f,
                0.01f, 1.0f);
            params.keySmoothness = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("smoothness")).toDouble(50.0)) / 1000.0f,
                0.001f, 1.0f);
            break;
        case FilterType::ChromaKey:
            params.keyMode = 2;
            parseKeyRgb(obj, params.keyR, params.keyG, params.keyB);
            params.keySimilarity = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("similarity")).toDouble(400.0))
                    / 1000.0f,
                0.01f, 1.0f);
            params.keySmoothness = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("smoothness")).toDouble(80.0)) / 1000.0f,
                0.001f, 1.0f);
            params.keySpill = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("spill")).toDouble(100.0)) / 1000.0f,
                0.0f, 1.0f);
            break;
        case FilterType::ColorGrade:
            params.gradeAmount = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("clut_amount")).toDouble(1.0)), 0.0f,
                1.0f);
            params.lift =
                static_cast<float>(obj.value(QStringLiteral("lift")).toDouble(0.0));
            params.gamma = std::max(
                0.01f, static_cast<float>(obj.value(QStringLiteral("gamma")).toDouble(1.0)));
            params.gain =
                static_cast<float>(obj.value(QStringLiteral("gain")).toDouble(1.0));
            params.lutPath = obj.value(QStringLiteral("image_path")).toString().toStdString();
            params.lutAmount = params.gradeAmount;
            break;
        case FilterType::ImageMask:
            params.maskEnabled = true;
            params.maskPath = obj.value(QStringLiteral("image_path")).toString().toStdString();
            params.maskOpacity = std::clamp(
                static_cast<float>(obj.value(QStringLiteral("opacity")).toDouble(1.0)), 0.0f,
                1.0f);
            break;
        case FilterType::RenderDelay:
            params.renderDelayMs = std::clamp(obj.value(QStringLiteral("delay_ms")).toInt(0), 0, 5000);
            break;
        default:
            break;
        }
    }
    return params;
}

void applyAudioFilters(const Source& source, AudioFrame& frame) {
    if (!frame.isValid()) {
        return;
    }
    for (const auto& filter : source.filters) {
        if (!filter.enabled) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
        const std::string key = source.id + ":" + filter.id;

        switch (filter.type) {
        case FilterType::Gain:
            applyGainDb(frame, static_cast<float>(obj.value(QStringLiteral("db")).toDouble(0.0)));
            break;
        case FilterType::Compressor:
            applyCompressor(key, frame,
                            static_cast<float>(obj.value(QStringLiteral("ratio")).toDouble(10.0)),
                            static_cast<float>(obj.value(QStringLiteral("threshold")).toDouble(-18.0)),
                            static_cast<float>(obj.value(QStringLiteral("attack_time")).toDouble(6.0)),
                            static_cast<float>(obj.value(QStringLiteral("release_time")).toDouble(60.0)),
                            static_cast<float>(obj.value(QStringLiteral("output_gain")).toDouble(0.0)));
            break;
        case FilterType::NoiseGate:
            applyNoiseGate(key, frame,
                           static_cast<float>(obj.value(QStringLiteral("open_threshold")).toDouble(-26.0)),
                           static_cast<float>(obj.value(QStringLiteral("close_threshold")).toDouble(-32.0)),
                           static_cast<float>(obj.value(QStringLiteral("attack_time")).toDouble(25.0)),
                           static_cast<float>(obj.value(QStringLiteral("hold_time")).toDouble(200.0)),
                           static_cast<float>(obj.value(QStringLiteral("release_time")).toDouble(150.0)));
            break;
        case FilterType::NoiseSuppress:
            applyNoiseSuppress(
                frame, static_cast<float>(obj.value(QStringLiteral("suppress_level")).toDouble( -30.0)));
            break;
        default:
            break;
        }
    }
}

} // namespace railshot
