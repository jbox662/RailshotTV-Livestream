#include "output/ReplayBuffer.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "output/FileRecorder.h"

#include <algorithm>

namespace railshot {

void ReplayBuffer::setSeconds(int seconds) {
    std::lock_guard lock(mutex_);
    seconds_ = std::clamp(seconds, 5, 300);
    trimLocked(std::max(1, AppSettings::instance().fps()));
}

void ReplayBuffer::clear() {
    std::lock_guard lock(mutex_);
    packets_.clear();
}

bool ReplayBuffer::empty() const {
    std::lock_guard lock(mutex_);
    return packets_.empty();
}

int ReplayBuffer::seconds() const {
    std::lock_guard lock(mutex_);
    return seconds_;
}

void ReplayBuffer::push(const EncodedPacket& packet) {
    std::lock_guard lock(mutex_);
    packets_.push_back(packet);
    trimLocked(std::max(1, AppSettings::instance().fps()));
}

void ReplayBuffer::trimLocked(int fps) {
    if (packets_.empty() || seconds_ <= 0 || fps <= 0) {
        return;
    }

    const int64_t maxVideoPts = static_cast<int64_t>(seconds_) * fps;
    int64_t latestVideoPts = -1;
    for (auto it = packets_.rbegin(); it != packets_.rend(); ++it) {
        if (it->isVideo) {
            latestVideoPts = it->pts;
            break;
        }
    }
    if (latestVideoPts < 0) {
        return;
    }

    const int64_t cutoff = latestVideoPts - maxVideoPts;
    while (!packets_.empty()) {
        const auto& front = packets_.front();
        if (front.isVideo && front.pts >= cutoff && front.isKeyFrame) {
            break;
        }
        if (!front.isVideo && front.pts >= cutoff) {
            // Keep looking for a video keyframe boundary — drop early audio with early video.
        }
        if (front.isVideo && front.pts < cutoff) {
            packets_.pop_front();
            continue;
        }
        if (!front.isVideo && (latestVideoPts - front.pts) > maxVideoPts + fps) {
            packets_.pop_front();
            continue;
        }
        // Drop until we can start on a keyframe at/after cutoff.
        if (front.isVideo && !front.isKeyFrame) {
            packets_.pop_front();
            continue;
        }
        break;
    }
}

bool ReplayBuffer::save(const std::string& filePath,
                        AVCodecContext* videoCtx,
                        AVCodecContext* audioCtx) {
    std::vector<EncodedPacket> snapshot;
    {
        std::lock_guard lock(mutex_);
        if (packets_.empty()) {
            return false;
        }
        snapshot.assign(packets_.begin(), packets_.end());
    }

    // Align start to first keyframe.
    size_t start = 0;
    while (start < snapshot.size() && !(snapshot[start].isVideo && snapshot[start].isKeyFrame)) {
        ++start;
    }
    if (start >= snapshot.size()) {
        Logger::warn("ReplayBuffer: no keyframe in buffer");
        return false;
    }

    FileRecorder recorder;
    if (!recorder.open(filePath, videoCtx, audioCtx)) {
        return false;
    }

    ThreadSafeQueue<EncodedPacket> queue;
    if (!recorder.start(&queue)) {
        recorder.close();
        return false;
    }

    for (size_t i = start; i < snapshot.size(); ++i) {
        queue.push(snapshot[i]);
    }
    queue.shutdown();
    recorder.stop();
    recorder.close();
    Logger::info("ReplayBuffer: saved " + filePath);
    return true;
}

} // namespace railshot
