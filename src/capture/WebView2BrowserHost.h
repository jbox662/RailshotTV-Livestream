#pragma once

#include <QObject>
#include <QImage>
#include <QString>

#include <functional>

class QTimer;
class QWidget;

namespace railshot {

// WebView2 browser host for canvas Browser Sources:
// - Width/Height are the browser render size
// - Custom CSS injected after page load
// - Frames captured repeatedly for the compositor (CDP screenshot first)
class WebView2BrowserHost : public QObject {
    Q_OBJECT

public:
    explicit WebView2BrowserHost(QObject* parent = nullptr);
    ~WebView2BrowserHost() override;

    [[nodiscard]] bool isAvailable() const { return available_; }
    [[nodiscard]] bool isReady() const { return ready_; }
    [[nodiscard]] bool hasPage() const { return pageReady_; }
    [[nodiscard]] int width() const { return width_; }
    [[nodiscard]] int height() const { return height_; }

    void ensureInitialized(int width, int height);
    void resizeView(int width, int height);
    void setCustomCss(const QString& css);
    void navigate(const QString& url);
    void forceNavigate(const QString& url);
    void reload();
    void capture(std::function<void(QImage)> callback);
    void startCaptureLoop(int intervalMs, std::function<void(QImage)> callback);
    void stopCaptureLoop();

private:
    void initializeEnvironment();
    void createController();
    void injectCustomCss();
    void onNavigationCompleted();
    void performCapture(std::function<void(QImage)> callback);
    void performCapturePreview(std::function<void(QImage)> callback);
    void performCaptureCdp(std::function<void(QImage)> callback, bool allowPreviewFallback);
    void deliverCapture(std::function<void(QImage)> callback, QImage image);

    QWidget* hostWidget_ = nullptr;
    QTimer* captureTimer_ = nullptr;
    void* environment_ = nullptr;
    void* controller_ = nullptr;
    void* webView_ = nullptr;

    int width_ = 800;
    int height_ = 600;
    QString currentUrl_;
    QString lastNavigatedUrl_;
    QString customCss_;
    bool available_ = false;
    bool ready_ = false;
    bool initializing_ = false;
    bool navigationPending_ = false;
    bool pageReady_ = false;
    bool captureBusy_ = false;
    std::function<void(QImage)> pendingCapture_;
    std::function<void(QImage)> loopCaptureCallback_;
};

[[nodiscard]] bool webView2RuntimeAvailable();

} // namespace railshot
