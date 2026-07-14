#pragma once

#include "core/FrameData.h"
#include "core/models/SourceTypes.h"
#include "encoder/FFmpegEncoder.h"
#include "output/FileRecorder.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace railshot {

class IsoRecorderSession {
public:
    IsoRecorderSession(std::string sourceId, std::string sourceName);
    ~IsoRecorderSession();

    bool start();
    void stop();
    void onRawFrame(const VideoFrame& frame);

    [[nodiscard]] bool isRecording() const { return running_.load(); }
    [[nodiscard]] std::string filePath() const;

private:
    void encodeThreadFunc();

    std::string sourceId_;
    std::string sourceName_;
    std::string filePath_;

    std::unique_ptr<FFmpegEncoder> encoder_;
    std::unique_ptr<FileRecorder> recorder_;
    ThreadSafeQueue<VideoFrame> videoInput_;
    ThreadSafeQueue<EncodedPacket> encodedOutput_;

    std::atomic<bool> running_{false};
};

class IsoRecorderManager {
public:
    static IsoRecorderManager& instance();

    void syncSources(const std::vector<Source>& sources);
    void stopAll();
    void onRawFrame(const std::string& sourceId, const std::string& sourceName,
                    const VideoFrame& frame);

    [[nodiscard]] bool isRecording() const;

private:
    IsoRecorderManager() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<IsoRecorderSession>> sessions_;
};

} // namespace railshot
