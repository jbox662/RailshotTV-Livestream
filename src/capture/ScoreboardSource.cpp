#include "capture/ScoreboardSource.h"

#include "core/Logger.h"
#include "score/ScoreManager.h"
#include "score/ScoreboardRenderer.h"
#include "score/ScoreboardStyle.h"

#include <QImage>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace railshot {

namespace {

constexpr int kDefaultWidth = 1920;
constexpr int kDefaultHeight = 110;

std::mutex g_scoreboardInstancesMutex;
std::vector<ScoreboardSource*> g_scoreboardInstances;

void imageToRgba(const QImage& image, VideoFrame& out) {
    QImage argb = image.format() == QImage::Format_ARGB32
                      ? image
                      : image.convertToFormat(QImage::Format_ARGB32);
    const int w = argb.width();
    const int h = argb.height();
    out.allocate(w, h, PixelFormat::RGBA32);
    for (int row = 0; row < h; ++row) {
        std::memcpy(out.data.data() + static_cast<size_t>(row) * static_cast<size_t>(w) * 4,
                    argb.constScanLine(row), static_cast<size_t>(w) * 4);
    }
}

} // namespace

ScoreboardSource::ScoreboardSource(Source config)
    : config_(std::move(config)) {
    registerInstance();
}

ScoreboardSource::~ScoreboardSource() {
    unregisterInstance();
    stop();
}

void ScoreboardSource::registerInstance() {
    std::lock_guard lock(g_scoreboardInstancesMutex);
    g_scoreboardInstances.push_back(this);
}

void ScoreboardSource::unregisterInstance() {
    std::lock_guard lock(g_scoreboardInstancesMutex);
    const auto it = std::find(g_scoreboardInstances.begin(), g_scoreboardInstances.end(), this);
    if (it != g_scoreboardInstances.end()) {
        g_scoreboardInstances.erase(it);
    }
}

void ScoreboardSource::invalidateFrame() {
    std::lock_guard lock(mutex_);
    lastVersion_ = 0;
    lastStyleKey_.clear();
    frame_.clear();
}

void ScoreboardSource::refreshAll() {
    std::vector<ScoreboardSource*> copy;
    {
        std::lock_guard lock(g_scoreboardInstancesMutex);
        copy = g_scoreboardInstances;
    }
    for (ScoreboardSource* source : copy) {
        if (source && source->isRunning()) {
            source->invalidateFrame();
            source->renderIfNeeded();
        }
    }
}

void ScoreboardSource::applyLiveConfig(const Source& source) {
    std::vector<ScoreboardSource*> copy;
    {
        std::lock_guard lock(g_scoreboardInstancesMutex);
        copy = g_scoreboardInstances;
    }
    for (ScoreboardSource* provider : copy) {
        if (provider && provider->config_.id == source.id) {
            provider->config_.overlaySettings = source.overlaySettings;
            provider->config_.transform.width = source.transform.width;
            provider->config_.transform.height = source.transform.height;
            provider->invalidateFrame();
            provider->renderIfNeeded();
        }
    }
}

bool ScoreboardSource::start() {
    running_ = true;
    renderIfNeeded();
    return true;
}

void ScoreboardSource::updateConfig(const Source& source) {
    config_ = source;
    invalidateFrame();
    if (running_.load()) {
        renderIfNeeded();
    }
}

void ScoreboardSource::stop() {
    running_ = false;
}

bool ScoreboardSource::isRunning() const {
    return running_.load();
}

bool ScoreboardSource::hasVideo() const {
    return running_.load();
}

bool ScoreboardSource::hasAudio() const {
    return false;
}

void ScoreboardSource::renderIfNeeded() {
    const uint64_t ver = ScoreManager::instance().version();
    const std::string styleKey = config_.overlaySettings;

    {
        std::lock_guard lock(mutex_);
        if (ver == lastVersion_ && styleKey == lastStyleKey_ && frame_.isValid()) {
            return;
        }
    }

    const MatchState state = ScoreManager::instance().state();
    const ScoreboardStyle style = ScoreboardStyle::fromJson(config_.overlaySettings);

    int w = style.barWidth > 0 ? style.barWidth
                               : static_cast<int>(config_.transform.width > 0 ? config_.transform.width : kDefaultWidth);
    int h = style.barHeight > 0 ? style.barHeight
                                : static_cast<int>(config_.transform.height > 0 ? config_.transform.height
                                                                                : kDefaultHeight);

    const QImage image = renderScoreboardImage(state, style, w, h);
    if (image.isNull()) {
        Logger::warn("ScoreboardSource: failed to render scoreboard image");
        return;
    }

    std::lock_guard lock(mutex_);
    imageToRgba(image, frame_);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame_.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    lastVersion_ = ver;
    lastStyleKey_ = styleKey;
}

std::optional<VideoFrame> ScoreboardSource::latestVideoFrame() {
    if (!running_.load()) {
        return std::nullopt;
    }
    std::lock_guard lock(mutex_);
    if (!frame_.isValid()) {
        return std::nullopt;
    }
    return frame_;
}

std::optional<AudioFrame> ScoreboardSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
