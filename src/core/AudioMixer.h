#pragma once

#include "core/FrameData.h"
#include "core/SourceRegistry.h"
#include "core/models/SceneManager.h"

#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace railshot {

class AudioMixer {
public:
    static float volumeToGain(int volumePercent);
    static void applyGain(AudioFrame& frame, float gain);

    /// Mix program audio and update per-strip peak meters (0..1 linear).
    [[nodiscard]] static std::optional<AudioFrame> mixActiveScene(SourceRegistry& registry);

    /// Snapshot of last computed peaks (ids like "__mic__" or source id).
    [[nodiscard]] static std::unordered_map<std::string, float> snapshotPeaks();

private:
    static float peakLevel(const AudioFrame& frame);
    static void storePeak(const std::string& id, float peak);
    static void decayUnusedPeaks(const std::unordered_map<std::string, float>& touched);
    static AudioFrame applySyncDelay(const std::string& id, AudioFrame frame, int delayMs);

    static std::mutex peakMutex_;
    static std::unordered_map<std::string, float> peaks_;

    static std::mutex delayMutex_;
    static std::unordered_map<std::string, std::deque<int16_t>> delayBuffers_;
};

} // namespace railshot
