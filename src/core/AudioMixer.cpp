#include "core/AudioMixer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace railshot {

std::mutex AudioMixer::peakMutex_;
std::unordered_map<std::string, float> AudioMixer::peaks_;

float AudioMixer::volumeToGain(int volumePercent) {
    const int clamped = std::clamp(volumePercent, 0, 100);
    if (clamped == 0) {
        return 0.0f;
    }
    // Map 0-100 to roughly -60 dB .. 0 dB (logarithmic)
    const float db = -60.0f + (static_cast<float>(clamped) / 100.0f) * 60.0f;
    return std::pow(10.0f, db / 20.0f);
}

void AudioMixer::applyGain(AudioFrame& frame, float gain) {
    if (gain <= 0.0f || frame.data.empty()) {
        std::fill(frame.data.begin(), frame.data.end(), 0);
        return;
    }

    auto* samples = reinterpret_cast<int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    for (size_t i = 0; i < count; ++i) {
        const float scaled = static_cast<float>(samples[i]) * gain;
        samples[i] = static_cast<int16_t>(std::clamp(static_cast<int>(scaled), -32768, 32767));
    }
}

float AudioMixer::peakLevel(const AudioFrame& frame) {
    if (frame.data.empty()) {
        return 0.0f;
    }
    const auto* samples = reinterpret_cast<const int16_t*>(frame.data.data());
    const size_t count = frame.data.size() / sizeof(int16_t);
    int maxAbs = 0;
    for (size_t i = 0; i < count; ++i) {
        maxAbs = std::max(maxAbs, std::abs(static_cast<int>(samples[i])));
    }
    return std::clamp(static_cast<float>(maxAbs) / 32768.0f, 0.0f, 1.0f);
}

void AudioMixer::storePeak(const std::string& id, float peak) {
    std::lock_guard lock(peakMutex_);
    auto& existing = peaks_[id];
    // Fast attack, slower visual hold is handled in the UI.
    existing = std::max(existing * 0.85f, peak);
}

void AudioMixer::decayUnusedPeaks(const std::unordered_map<std::string, float>& touched) {
    std::lock_guard lock(peakMutex_);
    for (auto& [id, level] : peaks_) {
        if (touched.find(id) == touched.end()) {
            level *= 0.82f;
            if (level < 0.001f) {
                level = 0.0f;
            }
        }
    }
}

std::unordered_map<std::string, float> AudioMixer::snapshotPeaks() {
    std::lock_guard lock(peakMutex_);
    return peaks_;
}

std::optional<AudioFrame> AudioMixer::mixActiveScene(SourceRegistry& registry) {
    auto& sceneManager = SceneManager::instance();
    const Scene* scene = sceneManager.activeScene();
    if (!scene) {
        return std::nullopt;
    }

    std::vector<AudioFrame> tracks;
    std::unordered_map<std::string, float> touched;

    if (auto mic = registry.latestMicFrame()) {
        AudioFrame micFrame = *mic;
        applyGain(micFrame, volumeToGain(100));
        storePeak("__mic__", peakLevel(micFrame));
        touched["__mic__"] = 1.0f;
        tracks.push_back(std::move(micFrame));
    } else {
        storePeak("__mic__", 0.0f);
        touched["__mic__"] = 1.0f;
    }

    for (const auto& src : scene->sources) {
        const bool audibleType = src.type == SourceType::MediaFile || src.type == SourceType::AudioDevice
                                 || src.type == SourceType::DesktopAudio || src.type == SourceType::NDI;
        if (!audibleType) {
            continue;
        }
        ISourceProvider* provider = registry.providerForSource(src.id);
        if (!provider || !provider->hasAudio()) {
            storePeak(src.id, 0.0f);
            touched[src.id] = 1.0f;
            continue;
        }
        auto audio = provider->latestAudioFrame();
        if (!audio.has_value() || src.muted || !src.isVisible) {
            storePeak(src.id, 0.0f);
            touched[src.id] = 1.0f;
            continue;
        }
        AudioFrame frame = *audio;
        applyGain(frame, volumeToGain(src.volume));
        storePeak(src.id, peakLevel(frame));
        touched[src.id] = 1.0f;
        tracks.push_back(std::move(frame));
    }

    decayUnusedPeaks(touched);

    if (tracks.empty()) {
        return std::nullopt;
    }

    AudioFrame mixed = tracks.front();
    for (size_t i = 1; i < tracks.size(); ++i) {
        const auto& track = tracks[i];
        const size_t sampleCount = std::min(mixed.data.size(), track.data.size());
        auto* out = reinterpret_cast<int16_t*>(mixed.data.data());
        const auto* in = reinterpret_cast<const int16_t*>(track.data.data());
        const size_t samples = sampleCount / sizeof(int16_t);
        for (size_t s = 0; s < samples; ++s) {
            const int sum = static_cast<int>(out[s]) + static_cast<int>(in[s]);
            out[s] = static_cast<int16_t>(std::clamp(sum, -32768, 32767));
        }
    }

    return mixed;
}

} // namespace railshot
