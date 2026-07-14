#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "encoder/FFmpegEncoder.h"
#include "core/ThreadSafeQueue.h"

#include <atomic>
#include <memory>
#include <string>
#include <thread>

namespace railshot {

class FileRecorder {
public:
    FileRecorder();
    ~FileRecorder();

    FileRecorder(const FileRecorder&) = delete;
    FileRecorder& operator=(const FileRecorder&) = delete;

    static std::string generateFilename(const std::string& prefix = "RailShot",
                                        const std::string& extension = "mp4");

    bool open(const std::string& filePath,
              AVCodecContext* videoCtx,
              AVCodecContext* audioCtx = nullptr,
              const std::string& formatName = {});
    void close();

    bool start(ThreadSafeQueue<EncodedPacket>* input);
    void stop();

    [[nodiscard]] bool isRecording() const { return running_.load(); }
    [[nodiscard]] std::string filePath() const { return filePath_; }
    [[nodiscard]] uint64_t bytesWritten() const { return bytesWritten_.load(); }

private:
    void recorderThreadFunc();
    bool writePacket(const EncodedPacket& packet);

    std::string filePath_;
    AVFormatContext* formatCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    AVCodecContext* videoCtx_ = nullptr;
    AVCodecContext* audioCtx_ = nullptr;

    ThreadSafeQueue<EncodedPacket>* input_ = nullptr;
    std::unique_ptr<std::thread> recorderThread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopRequested_{false};
    std::atomic<uint64_t> bytesWritten_{0};
};

} // namespace railshot
