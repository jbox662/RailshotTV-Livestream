#include "capture/BrowserSource.h"

#include "capture/BrowserSourceSettings.h"
#include "capture/WebView2BrowserHost.h"
#include "core/Logger.h"

#include <QColor>
#include <QFontDatabase>
#include <QImage>
#include <QPainter>
#include <QTextDocument>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace railshot {

namespace {

std::mutex g_browserInstancesMutex;
std::vector<BrowserSource*> g_browserInstances;

void imageToRgba(const QImage& image, VideoFrame& out) {
    // Keep Qt ARGB32 layout end-to-end for correct QPainter blending.
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

QString settingsKey(const BrowserSourceSettings& settings) {
    return settings.url + QLatin1Char('|') + QString::number(settings.width) + QLatin1Char('x')
           + QString::number(settings.height) + QLatin1Char('|') + settings.customCss
           + QLatin1Char('|') + (settings.isLocalFile ? QLatin1Char('1') : QLatin1Char('0'))
           + QLatin1Char('|') + (settings.fpsCustom ? QLatin1Char('1') : QLatin1Char('0'))
           + QLatin1Char('|') + QString::number(settings.fps)
           + QLatin1Char('|') + (settings.shutdownWhenNotVisible ? QLatin1Char('1') : QLatin1Char('0'))
           + QLatin1Char('|') + (settings.refreshWhenActive ? QLatin1Char('1') : QLatin1Char('0'))
           + QLatin1Char('|') + QString::number(static_cast<int>(settings.pagePermissions))
           + QLatin1Char('|') + (settings.rerouteAudio ? QLatin1Char('1') : QLatin1Char('0'));
}

QString wrapHtml(const QString& body) {
    return QStringLiteral(
               "<html><head><style>"
               "body{background:#1c1c20;color:#eee;font-family:Segoe UI,Arial,sans-serif;"
               "margin:16px;overflow:hidden;}"
               "</style></head><body>%1</body></html>")
        .arg(body);
}

QImage renderHtml(const QString& html, int width, int height) {
    QTextDocument doc;
    doc.setDefaultFont(QFontDatabase::systemFont(QFontDatabase::GeneralFont));
    doc.setHtml(html);
    doc.setTextWidth(static_cast<qreal>(width));

    QImage image(width, height, QImage::Format_ARGB32);
    image.fill(QColor(28, 28, 32));
    QPainter painter(&image);
    if (painter.isActive()) {
        painter.setRenderHint(QPainter::TextAntialiasing);
        doc.drawContents(&painter, QRectF(0, 0, width, height));
    }
    return image;
}

QImage placeholderImage(int width, int height, const QString& message) {
    return renderHtml(wrapHtml(QStringLiteral("<h2>Browser Source</h2><p>%1</p>").arg(message)),
                      width, height);
}

} // namespace

BrowserSource::BrowserSource(Source config)
    : config_(std::move(config)) {
    registerInstance();
    webHost_ = std::make_unique<WebView2BrowserHost>();
}

BrowserSource::~BrowserSource() {
    stopLiveRefresh();
    unregisterInstance();
    stop();
}

void BrowserSource::registerInstance() {
    std::lock_guard lock(g_browserInstancesMutex);
    g_browserInstances.push_back(this);
}

void BrowserSource::unregisterInstance() {
    std::lock_guard lock(g_browserInstancesMutex);
    const auto it = std::find(g_browserInstances.begin(), g_browserInstances.end(), this);
    if (it != g_browserInstances.end()) {
        g_browserInstances.erase(it);
    }
}

void BrowserSource::invalidateFrame() {
    std::lock_guard lock(mutex_);
    frame_.clear();
}

BrowserSourceSettings BrowserSource::settings() const {
    return BrowserSourceSettings::fromSource(config_);
}

void BrowserSource::refreshAll() {
    std::vector<BrowserSource*> copy;
    {
        std::lock_guard lock(g_browserInstancesMutex);
        copy = g_browserInstances;
    }
    for (BrowserSource* source : copy) {
        if (source && source->isRunning()) {
            source->requestCapture();
        }
    }
}

void BrowserSource::reloadAll() {
    std::vector<BrowserSource*> copy;
    {
        std::lock_guard lock(g_browserInstancesMutex);
        copy = g_browserInstances;
    }
    for (BrowserSource* source : copy) {
        if (source && source->isRunning() && source->webHost_) {
            source->webHost_->reload();
        }
    }
}

void BrowserSource::applyLiveConfig(const Source& source) {
    std::vector<BrowserSource*> copy;
    {
        std::lock_guard lock(g_browserInstancesMutex);
        copy = g_browserInstances;
    }
    for (BrowserSource* provider : copy) {
        if (!provider || provider->config_.id != source.id) {
            continue;
        }
        provider->updateConfig(source);
    }
}

bool BrowserSource::start() {
    running_ = true;
    const BrowserSourceSettings browserSettings = settings();
    applyCapturedImage(placeholderImage(
        browserSettings.width, browserSettings.height,
        QStringLiteral("Loading… %1").arg(browserSettings.url.toHtmlEscaped())));
    applySettingsToHost();
    syncNavigation();
    startLiveRefresh();
    return true;
}

void BrowserSource::updateConfig(const Source& source) {
    const BrowserSourceSettings before = settings();
    const bool wasVisible = config_.isVisible;
    config_ = source;
    if (!running_.load()) {
        return;
    }

    const BrowserSourceSettings after = settings();

    // Refresh browser when scene becomes active.
    if (after.refreshWhenActive && !wasVisible && config_.isVisible && webHost_) {
        webHost_->reload();
    }

    // Shutdown source when not visible — pause capture / clear output.
    if (after.shutdownWhenNotVisible && !config_.isVisible) {
        stopLiveRefresh();
        if (webHost_) {
            webHost_->stopCaptureLoop();
        }
        invalidateFrame();
        return;
    }

    // Resume after invisible shutdown.
    if (after.shutdownWhenNotVisible && !wasVisible && config_.isVisible) {
        applySettingsToHost();
        if (webHost_ && webHost_->isAvailable()) {
            if (after.refreshWhenActive) {
                webHost_->reload();
            } else if (!after.url.trimmed().isEmpty()) {
                webHost_->forceNavigate(after.url);
            }
        }
        startLiveRefresh();
        return;
    }

    const bool browserChanged = settingsKey(before) != settingsKey(after);
    if (!browserChanged) {
        if (config_.isVisible) {
            startLiveRefresh();
        }
        return;
    }

    lastSettingsKey_.clear();
    applyCapturedImage(placeholderImage(
        after.width, after.height, QStringLiteral("Updating… %1").arg(after.url.toHtmlEscaped())));
    applySettingsToHost();
    if (webHost_ && !after.url.trimmed().isEmpty()) {
        webHost_->forceNavigate(after.url);
    } else {
        syncNavigation();
    }
    startLiveRefresh();
}

void BrowserSource::stop() {
    running_ = false;
    stopLiveRefresh();
    if (webHost_) {
        webHost_->stopCaptureLoop();
    }
}

bool BrowserSource::isRunning() const {
    return running_.load();
}

bool BrowserSource::hasVideo() const {
    return running_.load();
}

bool BrowserSource::hasAudio() const {
    return false;
}

void BrowserSource::applySettingsToHost() {
    if (!webHost_) {
        return;
    }
    const BrowserSourceSettings browserSettings = settings();
    // Browser render size from properties, not canvas scale.
    webHost_->ensureInitialized(browserSettings.width, browserSettings.height);
    webHost_->resizeView(browserSettings.width, browserSettings.height);
    webHost_->setCustomCss(browserSettings.customCss);
}

void BrowserSource::syncNavigation() {
    if (!running_.load()) {
        return;
    }

    const BrowserSourceSettings browserSettings = settings();
    const std::string key = settingsKey(browserSettings).toStdString();
    if (key == lastSettingsKey_) {
        return;
    }
    lastSettingsKey_ = key;

    applySettingsToHost();

    if (!(webHost_ && webHost_->isAvailable())) {
        refreshFallbackContent();
        return;
    }

    if (browserSettings.url.trimmed().isEmpty()) {
        refreshFallbackContent();
        return;
    }

    webHost_->navigate(browserSettings.url);
}

void BrowserSource::startLiveRefresh() {
    if (!webHost_) {
        return;
    }

    const BrowserSourceSettings browserSettings = settings();
    if (browserSettings.shutdownWhenNotVisible && !config_.isVisible) {
        webHost_->stopCaptureLoop();
        return;
    }

    // Continuous frames (custom FPS when enabled, else canvas FPS).
    webHost_->startCaptureLoop(browserSettings.captureIntervalMs(), [this](QImage image) {
        if (!image.isNull()) {
            applyCapturedImage(image);
        }
        captureInFlight_ = false;
    });

    // Re-apply CSS periodically so SPA routes keep transparent backgrounds.
    if (webHost_ && !browserSettings.customCss.trimmed().isEmpty()) {
        webHost_->setCustomCss(browserSettings.customCss);
    }

    if (liveTimer_) {
        return;
    }
    liveTimer_ = new QTimer(webHost_.get());
    liveTimer_->setInterval(500);
    QObject::connect(liveTimer_, &QTimer::timeout, liveTimer_, [this]() {
        if (!running_.load()) {
            return;
        }
        // If page is loading, nudge a capture; otherwise the capture loop owns it.
        if (webHost_ && webHost_->isReady() && !webHost_->hasPage()) {
            requestCapture();
        }
        if (!(webHost_ && webHost_->isAvailable())) {
            refreshFallbackContent();
        }
    });
    liveTimer_->start();
}

void BrowserSource::stopLiveRefresh() {
    if (liveTimer_) {
        liveTimer_->stop();
        liveTimer_->deleteLater();
        liveTimer_ = nullptr;
    }
}

void BrowserSource::applyCapturedImage(const QImage& image) {
    if (image.isNull()) {
        captureInFlight_ = false;
        return;
    }

    std::lock_guard lock(mutex_);
    imageToRgba(image, frame_);
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    frame_.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
    captureInFlight_ = false;
}

void BrowserSource::requestCapture() {
    if (!running_.load() || !webHost_) {
        return;
    }
    if (captureInFlight_) {
        return;
    }
    captureInFlight_ = true;
    webHost_->capture([this](QImage image) {
        if (!image.isNull()) {
            applyCapturedImage(image);
            return;
        }
        captureInFlight_ = false;
    });
}

void BrowserSource::refreshFallbackContent() {
    const BrowserSourceSettings browserSettings = settings();
    if (!(webHost_ && webHost_->isAvailable())) {
        applyCapturedImage(placeholderImage(
            browserSettings.width, browserSettings.height,
            QStringLiteral("WebView2 runtime required. Install Edge WebView2, then reopen this "
                           "source.")));
        return;
    }
    if (browserSettings.url.trimmed().isEmpty()) {
        applyCapturedImage(placeholderImage(browserSettings.width, browserSettings.height,
                                            QStringLiteral("Set a URL in Properties.")));
    }
}

std::optional<VideoFrame> BrowserSource::latestVideoFrame() {
    if (!running_.load()) {
        return std::nullopt;
    }
    const BrowserSourceSettings browserSettings = settings();
    if (browserSettings.shutdownWhenNotVisible && !config_.isVisible) {
        return std::nullopt;
    }
    std::lock_guard lock(mutex_);
    if (!frame_.isValid()) {
        return std::nullopt;
    }
    return frame_;
}

std::optional<AudioFrame> BrowserSource::latestAudioFrame() {
    return std::nullopt;
}

} // namespace railshot
