#include "output/FileRecorder.h"

#include "core/Logger.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace railshot {

FileRecorder::FileRecorder() = default;

FileRecorder::~FileRecorder() {
    stop();
    close();
}

std::string FileRecorder::generateFilename(const std::string& prefix) {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &t);

    std::ostringstream oss;
    oss << prefix << '_'
        << std::put_time(&local, "%Y-%m-%d_%H-%M-%S") << ".mp4";
    return oss.str();
}

bool FileRecorder::open(const std::string& filePath,
                        AVCodecContext* videoCtx,
                        AVCodecContext* audioCtx) {
    close();
    if (!videoCtx) {
        return false;
    }

    filePath_ = filePath;
    videoCtx_ = videoCtx;
    audioCtx_ = audioCtx;

    const std::filesystem::path path(filePath_);
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    int ret = avformat_alloc_output_context2(&formatCtx_, nullptr, nullptr, filePath_.c_str());
    if (ret < 0 || !formatCtx_) {
        Logger::error("FileRecorder: failed to create output context");
        return false;
    }

    AVStream* videoStream = avformat_new_stream(formatCtx_, nullptr);
    if (!videoStream) {
        Logger::error("FileRecorder: failed to create video stream");
        close();
        return false;
    }
    videoStreamIndex_ = videoStream->index;
    avcodec_parameters_from_context(videoStream->codecpar, videoCtx_);
    videoStream->time_base = videoCtx_->time_base;

    AVStream* audioStream = nullptr;
    if (audioCtx_) {
        audioStream = avformat_new_stream(formatCtx_, nullptr);
        if (!audioStream) {
            Logger::error("FileRecorder: failed to create audio stream");
            close();
            return false;
        }
        audioStreamIndex_ = audioStream->index;
        avcodec_parameters_from_context(audioStream->codecpar, audioCtx_);
        audioStream->time_base = audioCtx_->time_base;
    }

    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&formatCtx_->pb, filePath_.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errBuf[256];
            av_strerror(ret, errBuf, sizeof(errBuf));
            Logger::error(std::string("FileRecorder: avio_open failed: ") + errBuf);
            close();
            return false;
        }
    }

    ret = avformat_write_header(formatCtx_, nullptr);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::error(std::string("FileRecorder: write_header failed: ") + errBuf);
        close();
        return false;
    }

    Logger::info("FileRecorder: recording to " + filePath_);
    return true;
}

void FileRecorder::close() {
    if (formatCtx_) {
        if (formatCtx_->pb) {
            avio_flush(formatCtx_->pb);
        }
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }
    filePath_.clear();
    videoCtx_ = nullptr;
    audioCtx_ = nullptr;
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    bytesWritten_ = 0;
}

bool FileRecorder::start(ThreadSafeQueue<EncodedPacket>* input) {
    if (running_.load() || !input || !formatCtx_) {
        return false;
    }

    input_ = input;
    stopRequested_ = false;
    running_ = true;
    recorderThread_ = std::make_unique<std::thread>(&FileRecorder::recorderThreadFunc, this);
    return true;
}

void FileRecorder::stop() {
    stopRequested_ = true;
    if (input_) {
        input_->shutdown();
    }
    if (recorderThread_ && recorderThread_->joinable()) {
        recorderThread_->join();
    }
    recorderThread_.reset();

    if (formatCtx_ && running_.load()) {
        av_write_trailer(formatCtx_);
        if (formatCtx_->pb) {
            avio_closep(&formatCtx_->pb);
        }
    }

    running_ = false;
    input_ = nullptr;
}

bool FileRecorder::writePacket(const EncodedPacket& packet) {
    if (!formatCtx_) {
        return false;
    }

    AVPacket avPkt;
    av_init_packet(&avPkt);
    avPkt.data = const_cast<uint8_t*>(packet.data.data());
    avPkt.size = static_cast<int>(packet.data.size());
    avPkt.pts = packet.pts;
    avPkt.dts = packet.dts;

    if (packet.isVideo) {
        avPkt.stream_index = videoStreamIndex_;
        if (packet.isKeyFrame) {
            avPkt.flags |= AV_PKT_FLAG_KEY;
        }
        avPkt.duration = 1;
    } else {
        avPkt.stream_index = audioStreamIndex_;
    }

    const int ret = av_interleaved_write_frame(formatCtx_, &avPkt);
    if (ret < 0) {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        Logger::warn(std::string("FileRecorder: write_frame failed: ") + errBuf);
        return false;
    }

    bytesWritten_.fetch_add(packet.data.size());
    return true;
}

void FileRecorder::recorderThreadFunc() {
    while (!stopRequested_.load()) {
        auto packetOpt = input_->pop(100);
        if (!packetOpt.has_value()) {
            continue;
        }
        writePacket(*packetOpt);
    }
    running_ = false;
}

} // namespace railshot
