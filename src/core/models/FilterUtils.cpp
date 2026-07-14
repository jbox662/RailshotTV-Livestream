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
    for (const auto& filter : source.filters) {
        if (!filter.enabled) {
            continue;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(filter.paramsJson));
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
        if (filter.type == FilterType::Opacity) {
            const float opacity = static_cast<float>(obj.value(QStringLiteral("opacity")).toDouble(1.0));
            params.opacity *= std::clamp(opacity, 0.0f, 1.0f);
        } else if (filter.type == FilterType::ColorCorrection) {
            params.brightness = static_cast<float>(obj.value(QStringLiteral("brightness")).toDouble(0.0));
            params.contrast = static_cast<float>(obj.value(QStringLiteral("contrast")).toDouble(1.0));
            params.saturation = static_cast<float>(obj.value(QStringLiteral("saturation")).toDouble(1.0));
            params.brightness = std::clamp(params.brightness, -1.0f, 1.0f);
            params.contrast = std::clamp(params.contrast, 0.0f, 2.0f);
            params.saturation = std::clamp(params.saturation, 0.0f, 2.0f);
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
