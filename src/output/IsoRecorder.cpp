#include "output/IsoRecorder.h"

#include "core/Logger.h"

#include <QStandardPaths>

#include <filesystem>

namespace railshot {

namespace {

std::string recordingsDir() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
        + "/RailShot/ISO";
    std::filesystem::create_directories(dir.toStdString());
    return dir.toStdString();
}

std::string sanitizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            out.push_back(c);
        } else if (c == ' ') {
            out.push_back('_');
        }
    }
    return out.empty() ? "Source" : out;
}

} // namespace

IsoRecorderSession::IsoRecorderSession(std::string sourceId, std::string sourceName)
    : sourceId_(std::move(sourceId))
    , sourceName_(std::move(sourceName)) {}

IsoRecorderSession::~IsoRecorderSession() {
    stop();
}

bool IsoRecorderSession::start() {
    if (running_.load()) {
        return true;
    }

    filePath_ = recordingsDir() + "/" + sanitizeName(sourceName_) + "_"
        + FileRecorder::generateFilename("ISO");

    encoder_ = std::make_unique<FFmpegEncoder>();
    recorder_ = std::make_unique<FileRecorder>();

    if (!encoder_->initialize()) {
        Logger::error("IsoRecorder: encoder init failed for " + sourceName_);
        encoder_.reset();
        recorder_.reset();
        return false;
    }

    if (!recorder_->open(filePath_, encoder_->videoCodecContext(), nullptr)) {
        Logger::error("IsoRecorder: file open failed for " + sourceName_);
        encoder_->shutdown();
        encoder_.reset();
        recorder_.reset();
        return false;
    }

    videoInput_.reset();
    encodedOutput_.reset();

    if (!encoder_->start(&videoInput_, nullptr, &encodedOutput_)) {
        Logger::error("IsoRecorder: encoder start failed for " + sourceName_);
        recorder_->close();
        encoder_->shutdown();
        encoder_.reset();
        recorder_.reset();
        return false;
    }

    if (!recorder_->start(&encodedOutput_)) {
        Logger::error("IsoRecorder: recorder start failed for " + sourceName_);
        encoder_->stop();
        encoder_->shutdown();
        encoder_.reset();
        recorder_.reset();
        return false;
    }

    running_ = true;
    Logger::info("IsoRecorder: started ISO recording for " + sourceName_ + " -> " + filePath_);
    return true;
}

void IsoRecorderSession::stop() {
    if (!running_.load()) {
        return;
    }

    videoInput_.shutdown();
    recorder_->stop();
    encoder_->stop();
    encoder_->shutdown();

    recorder_.reset();
    encoder_.reset();
    running_ = false;
    Logger::info("IsoRecorder: stopped ISO recording for " + sourceName_);
}

void IsoRecorderSession::onRawFrame(const VideoFrame& frame) {
    if (!running_.load() || !frame.isValid()) {
        return;
    }
    videoInput_.push(frame);
}

std::string IsoRecorderSession::filePath() const {
    return filePath_;
}

IsoRecorderManager& IsoRecorderManager::instance() {
    static IsoRecorderManager manager;
    return manager;
}

void IsoRecorderManager::syncSources(const std::vector<Source>& sources) {
    std::lock_guard lock(mutex_);

    std::unordered_map<std::string, std::unique_ptr<IsoRecorderSession>> updated;
    for (const auto& src : sources) {
        if (!src.isoRecording || !src.isVisible) {
            continue;
        }
        if (src.type != SourceType::VideoDevice && src.type != SourceType::NDI) {
            continue;
        }

        auto existing = sessions_.find(src.id);
        if (existing != sessions_.end()) {
            updated[src.id] = std::move(existing->second);
            sessions_.erase(existing);
            continue;
        }

        auto session = std::make_unique<IsoRecorderSession>(src.id, src.name);
        if (session->start()) {
            updated[src.id] = std::move(session);
        }
    }

    for (auto& entry : sessions_) {
        entry.second->stop();
    }
    sessions_ = std::move(updated);
}

void IsoRecorderManager::stopAll() {
    std::lock_guard lock(mutex_);
    for (auto& entry : sessions_) {
        entry.second->stop();
    }
    sessions_.clear();
}

void IsoRecorderManager::onRawFrame(const std::string& sourceId, const std::string& sourceName,
                                    const VideoFrame& frame) {
    std::lock_guard lock(mutex_);
    const auto it = sessions_.find(sourceId);
    if (it != sessions_.end()) {
        it->second->onRawFrame(frame);
    }
}

bool IsoRecorderManager::isRecording() const {
    std::lock_guard lock(mutex_);
    return !sessions_.empty();
}

} // namespace railshot
