#include "core/SourceRegistry.h"

#include "core/AppSettings.h"
#include "core/Logger.h"
#include "output/IsoRecorder.h"

#include <unordered_map>

namespace railshot {

SourceRegistry::SourceRegistry() = default;

SourceRegistry::~SourceRegistry() {
    stopAll();
}

std::unique_ptr<ISourceProvider> SourceRegistry::createProvider(const Source& source) {
    switch (source.type) {
    case SourceType::VideoDevice:
        return std::make_unique<VideoDeviceSource>(source);
    case SourceType::Image:
        return std::make_unique<ImageSource>(source);
    case SourceType::MediaFile:
        return std::make_unique<MediaSource>(source);
    case SourceType::AudioDevice:
        return std::make_unique<AudioDeviceSource>(source);
    case SourceType::NDI:
        return std::make_unique<NdiSource>(source);
    case SourceType::Browser:
        return std::make_unique<BrowserSource>(source);
    case SourceType::Scoreboard:
        return std::make_unique<ScoreboardSource>(source);
    case SourceType::DisplayCapture:
        return std::make_unique<DisplayCaptureSource>(source);
    case SourceType::WindowCapture:
        return std::make_unique<WindowCaptureSource>(source);
    case SourceType::Text:
        return std::make_unique<TextSource>(source);
    case SourceType::Color:
        return std::make_unique<ColorSource>(source);
    case SourceType::DesktopAudio:
        return std::make_unique<DesktopAudioSource>(source);
    case SourceType::ApplicationAudio:
        return std::make_unique<ApplicationAudioSource>(source);
    case SourceType::GameCapture:
        return std::make_unique<GameCaptureSource>(source);
    }
    return nullptr;
}

bool SourceRegistry::canReuseProvider(const ISourceProvider& provider, const Source& source) {
    const Source& current = provider.config();
    if (current.id != source.id || current.type != source.type) {
        return false;
    }
    // Live overlays / media / audio can update in place (visibility, speed, device id).
    if (source.type == SourceType::Browser || source.type == SourceType::Scoreboard
        || source.type == SourceType::Text || source.type == SourceType::Color
        || source.type == SourceType::MediaFile || source.type == SourceType::AudioDevice
        || source.type == SourceType::DesktopAudio || source.type == SourceType::ApplicationAudio
        || source.type == SourceType::GameCapture || source.type == SourceType::WindowCapture) {
        return true;
    }
    return current.pathOrDeviceId == source.pathOrDeviceId;
}

void SourceRegistry::syncScene(const Scene& scene) {
    std::lock_guard lock(mutex_);

    std::unordered_map<std::string, std::unique_ptr<ISourceProvider>> updated;
    for (const auto& src : scene.sources) {
        auto existing = providers_.find(src.id);
        if (existing != providers_.end() && canReuseProvider(*existing->second, src)) {
            auto provider = std::move(existing->second);
            providers_.erase(existing);
            provider->updateConfig(src);
            if (!provider->isRunning()) {
                if (!provider->start()) {
                    Logger::warn("SourceRegistry: failed to restart source " + src.name);
                    continue;
                }
            }
            updated[src.id] = std::move(provider);
            continue;
        }

        if (existing != providers_.end()) {
            existing->second->stop();
            providers_.erase(existing);
        }

        auto provider = createProvider(src);
        if (!provider) {
            continue;
        }
        if (provider->start()) {
            updated[src.id] = std::move(provider);
        } else {
            Logger::warn("SourceRegistry: failed to start source " + src.name);
        }
    }

    for (auto& [id, provider] : providers_) {
        provider->stop();
    }

    providers_ = std::move(updated);
    IsoRecorderManager::instance().syncSources(scene.sources);
}

void SourceRegistry::syncCombinedScenes(const Scene& sceneA, const Scene& sceneB) {
    Scene merged;
    merged.id = "combined";
    merged.name = "Combined";

    std::unordered_map<std::string, Source> unique;
    for (const auto& src : sceneA.sources) {
        unique[src.id] = src;
    }
    for (const auto& src : sceneB.sources) {
        unique[src.id] = src;
    }
    for (const auto& [id, src] : unique) {
        merged.sources.push_back(src);
    }
    syncScene(merged);
}

void SourceRegistry::stopAll() {
    stopMicCapture();
    IsoRecorderManager::instance().stopAll();
    std::lock_guard lock(mutex_);
    for (auto& [id, provider] : providers_) {
        provider->stop();
    }
    providers_.clear();
}

ISourceProvider* SourceRegistry::providerForSource(const std::string& sourceId) {
    std::lock_guard lock(mutex_);
    const auto it = providers_.find(sourceId);
    return it != providers_.end() ? it->second.get() : nullptr;
}

std::vector<std::string> SourceRegistry::activeSourceIds() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> ids;
    ids.reserve(providers_.size());
    for (const auto& [id, provider] : providers_) {
        ids.push_back(id);
    }
    return ids;
}

bool SourceRegistry::startMicCapture() {
    if (micRunning_) {
        return true;
    }
    micCapture_ = std::make_unique<WasapiAudioCapture>();
    const std::string deviceId = AppSettings::instance().data().micDeviceId;
    if (!micCapture_->openMicrophone(deviceId)) {
        micCapture_.reset();
        return false;
    }
    micQueue_.reset();
    if (!micCapture_->start(&micQueue_)) {
        micCapture_.reset();
        return false;
    }
    micRunning_ = true;
    return true;
}

void SourceRegistry::stopMicCapture() {
    if (!micRunning_) {
        return;
    }
    if (micCapture_) {
        micCapture_->stop();
    }
    micQueue_.shutdown();
    micCapture_.reset();
    micRunning_ = false;
}

bool SourceRegistry::restartMicCapture() {
    stopMicCapture();
    return startMicCapture();
}

std::optional<AudioFrame> SourceRegistry::latestMicFrame() {
    std::optional<AudioFrame> latest;
    while (auto frame = micQueue_.pop(0)) {
        latest = std::move(*frame);
    }
    return latest;
}

} // namespace railshot
