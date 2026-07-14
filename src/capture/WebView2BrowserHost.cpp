#include "capture/WebView2BrowserHost.h"

#include "core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QWidget>

#include <algorithm>
#include <cmath>

#ifdef RAILSHOT_HAS_WEBVIEW2
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>
#include <Shlwapi.h>
#include <objbase.h>
#include <vector>
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "User32.lib")
#endif

namespace railshot {

namespace {

#ifdef RAILSHOT_HAS_WEBVIEW2
using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

QString normalizeBrowserUrl(const QString& location) {
    const QString trimmed = location.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    if (trimmed.startsWith(QLatin1String("http://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)
        || trimmed.startsWith(QLatin1String("data:"), Qt::CaseInsensitive)) {
        return trimmed;
    }

    QFileInfo info(trimmed);
    if (info.exists()) {
        return QUrl::fromLocalFile(info.absoluteFilePath()).toString(QUrl::FullyEncoded);
    }

    const QUrl guessed = QUrl::fromUserInput(trimmed, QCoreApplication::applicationDirPath());
    if (guessed.isValid()) {
        return guessed.toString(QUrl::FullyEncoded);
    }
    return trimmed;
}

// Park an opaque HWND just outside the primary work area. Layered alpha=1 windows often
// never receive real compositor paint, so CapturePreview returns solid gray forever.
void placeHostForCapture(HWND hwnd, int width, int height) {
    int x = -width - 80;
    int y = 0;
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        x = avail.right() + 40;
        y = avail.top() + 40;
    }

    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    ex |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    ex &= ~(WS_EX_LAYERED | WS_EX_TRANSPARENT);
    SetWindowLongW(hwnd, GWL_EXSTYLE, ex);
    SetWindowPos(hwnd, HWND_BOTTOM, x, y, width, height,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

bool isUselessCapture(const QImage& image) {
    if (image.isNull() || image.width() < 2 || image.height() < 2) {
        return true;
    }

    // Scoreholio overlays are often mostly transparent — do NOT reject on alpha.
    // Reject uniform CapturePreview failures (solid gray / black / white wash).
    const QImage probe = image.scaled(32, 32, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    int samples = 0;
    qint64 sumR = 0;
    qint64 sumG = 0;
    qint64 sumB = 0;
    qint64 sumA = 0;
    int opaque = 0;
    for (int y = 0; y < probe.height(); ++y) {
        for (int x = 0; x < probe.width(); ++x) {
            const QRgb p = probe.pixel(x, y);
            const int a = qAlpha(p);
            sumA += a;
            if (a > 8) {
                ++opaque;
                sumR += qRed(p);
                sumG += qGreen(p);
                sumB += qBlue(p);
                ++samples;
            }
        }
    }

    // Fully empty transparent buffer.
    if (opaque == 0 || sumA == 0) {
        return true;
    }

    const double inv = 1.0 / static_cast<double>(samples);
    const double avgR = sumR * inv;
    const double avgG = sumG * inv;
    const double avgB = sumB * inv;

    double variance = 0.0;
    for (int y = 0; y < probe.height(); ++y) {
        for (int x = 0; x < probe.width(); ++x) {
            const QRgb p = probe.pixel(x, y);
            if (qAlpha(p) <= 8) {
                continue;
            }
            const double dr = qRed(p) - avgR;
            const double dg = qGreen(p) - avgG;
            const double db = qBlue(p) - avgB;
            variance += dr * dr + dg * dg + db * db;
        }
    }
    variance /= static_cast<double>(samples);

    // Solid gray CapturePreview wash ≈ low variance.
    if (variance < 12.0) {
        return true;
    }
    return false;
}

QImage decodeCdpPng(LPCWSTR returnJson) {
    if (!returnJson) {
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(QString::fromWCharArray(returnJson).toUtf8());
    if (!doc.isObject()) {
        return {};
    }
    const QByteArray png =
        QByteArray::fromBase64(doc.object().value(QStringLiteral("data")).toString().toUtf8());
    if (png.isEmpty()) {
        return {};
    }
    return QImage::fromData(png, "PNG");
}
#endif

} // namespace

bool webView2RuntimeAvailable() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    LPWSTR version = nullptr;
    const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &version);
    if (SUCCEEDED(hr) && version) {
        CoTaskMemFree(version);
        return true;
    }
#endif
    return false;
}

WebView2BrowserHost::WebView2BrowserHost(QObject* parent)
    : QObject(parent) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    available_ = webView2RuntimeAvailable();
    Logger::info(available_ ? "WebView2BrowserHost: runtime available"
                            : "WebView2BrowserHost: runtime NOT available");
#else
    available_ = false;
    Logger::error("WebView2BrowserHost: built without RAILSHOT_HAS_WEBVIEW2");
#endif
}

WebView2BrowserHost::~WebView2BrowserHost() {
    stopCaptureLoop();
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (controller_) {
        static_cast<ICoreWebView2Controller*>(controller_)->Close();
        controller_ = nullptr;
    }
    webView_ = nullptr;
    environment_ = nullptr;
#endif
    delete hostWidget_;
    hostWidget_ = nullptr;
}

void WebView2BrowserHost::ensureInitialized(int width, int height) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    if (ready_) {
        resizeView(width_, height_);
        return;
    }
    if (!available_ || initializing_) {
        return;
    }
    initializing_ = true;

    hostWidget_ = new QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    hostWidget_->setAttribute(Qt::WA_ShowWithoutActivating, true);
    hostWidget_->setAttribute(Qt::WA_QuitOnClose, false);
    hostWidget_->setAttribute(Qt::WA_NativeWindow, true);
    hostWidget_->setFixedSize(width_, height_);
    hostWidget_->setStyleSheet(QStringLiteral("background: #000000;"));
    hostWidget_->show();
    hostWidget_->createWinId();

    placeHostForCapture(reinterpret_cast<HWND>(hostWidget_->winId()), width_, height_);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    initializeEnvironment();
#else
    Q_UNUSED(width);
    Q_UNUSED(height);
#endif
}

void WebView2BrowserHost::resizeView(int width, int height) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    width_ = std::max(1, width);
    height_ = std::max(1, height);
    if (hostWidget_) {
        hostWidget_->setFixedSize(width_, height_);
        placeHostForCapture(reinterpret_cast<HWND>(hostWidget_->winId()), width_, height_);
    }
    if (controller_) {
        RECT bounds{0, 0, width_, height_};
        auto* controller = static_cast<ICoreWebView2Controller*>(controller_);
        controller->put_Bounds(bounds);
        controller->put_IsVisible(TRUE);
        controller->NotifyParentWindowPositionChanged();
    }
#else
    Q_UNUSED(width);
    Q_UNUSED(height);
#endif
}

void WebView2BrowserHost::setCustomCss(const QString& css) {
    customCss_ = css;
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (ready_ && pageReady_) {
        injectCustomCss();
    }
#endif
}

void WebView2BrowserHost::initializeEnvironment() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    const QString userData =
        QDir(QCoreApplication::applicationDirPath()).filePath("webview2-data");
    QDir().mkpath(userData);

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userData.toStdWString().c_str(), nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(result) || !env) {
                    Logger::error("WebView2BrowserHost: failed to create environment hr="
                                  + std::to_string(result));
                    initializing_ = false;
                    available_ = false;
                    return result;
                }
                environment_ = env;
                env->AddRef();
                QMetaObject::invokeMethod(this, [this]() { createController(); },
                                          Qt::QueuedConnection);
                return S_OK;
            })
            .Get());
#endif
}

void WebView2BrowserHost::injectCustomCss() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!webView_ || customCss_.isEmpty()) {
        return;
    }

    const QString encoded = QString::fromUtf8(QUrl::toPercentEncoding(customCss_));
    const QString script = QStringLiteral(
                               "(function(){"
                               "let s=document.getElementById('railshotCSS');"
                               "if(!s){s=document.createElement('style');s.id='railshotCSS';"
                               "(document.head||document.documentElement).appendChild(s);}"
                               "s.textContent=decodeURIComponent('%1');"
                               "})();")
                               .arg(encoded);

    static_cast<ICoreWebView2*>(webView_)->ExecuteScript(script.toStdWString().c_str(), nullptr);
#endif
}

void WebView2BrowserHost::createController() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!environment_ || !hostWidget_) {
        initializing_ = false;
        return;
    }

    auto* env = static_cast<ICoreWebView2Environment*>(environment_);
    const HWND hwnd = reinterpret_cast<HWND>(hostWidget_->winId());
    placeHostForCapture(hwnd, width_, height_);

    env->CreateCoreWebView2Controller(
        hwnd, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                  [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                      if (FAILED(result) || !controller) {
                          Logger::error("WebView2BrowserHost: failed to create controller hr="
                                        + std::to_string(result));
                          initializing_ = false;
                          available_ = false;
                          return result;
                      }

                      controller_ = controller;
                      controller->AddRef();
                      controller->get_CoreWebView2(
                          reinterpret_cast<ICoreWebView2**>(&webView_));

                      RECT bounds{0, 0, width_, height_};
                      controller->put_Bounds(bounds);
                      controller->put_IsVisible(TRUE);

                      ComPtr<ICoreWebView2Controller2> controller2;
                      if (SUCCEEDED(controller->QueryInterface(IID_PPV_ARGS(&controller2)))
                          && controller2) {
                          // Transparent background for overlay pages (e.g. Scoreholio).
                          COREWEBVIEW2_COLOR bg{};
                          bg.A = 0;
                          bg.R = 0;
                          bg.G = 0;
                          bg.B = 0;
                          controller2->put_DefaultBackgroundColor(bg);
                      }

                      auto* webview = static_cast<ICoreWebView2*>(webView_);
                      webview->AddRef();
                      webview->CallDevToolsProtocolMethod(L"Page.enable", L"{}", nullptr);

                      webview->add_NavigationCompleted(
                          Callback<ICoreWebView2NavigationCompletedEventHandler>(
                              [this](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args)
                                  -> HRESULT {
                                  BOOL success = FALSE;
                                  args->get_IsSuccess(&success);
                                  navigationPending_ = false;
                                  pageReady_ = success == TRUE;
                                  Logger::info(success ? "WebView2BrowserHost: navigation OK"
                                                       : "WebView2BrowserHost: navigation FAILED");
                                  QMetaObject::invokeMethod(
                                      this,
                                      [this]() {
                                          injectCustomCss();
                                          // SPA overlays (Scoreholio) need time after NavCompleted.
                                          onNavigationCompleted();
                                          QTimer::singleShot(600, this, [this]() {
                                              onNavigationCompleted();
                                          });
                                          QTimer::singleShot(1500, this, [this]() {
                                              onNavigationCompleted();
                                          });
                                          QTimer::singleShot(3000, this, [this]() {
                                              onNavigationCompleted();
                                          });
                                      },
                                      Qt::QueuedConnection);
                                  return S_OK;
                              })
                              .Get(),
                          nullptr);

                      ComPtr<ICoreWebView2Settings> settings;
                      if (SUCCEEDED(webview->get_Settings(&settings)) && settings) {
                          settings->put_IsScriptEnabled(TRUE);
                          settings->put_AreDefaultScriptDialogsEnabled(FALSE);
                          settings->put_IsWebMessageEnabled(TRUE);
                          settings->put_IsStatusBarEnabled(FALSE);
                          settings->put_AreDevToolsEnabled(TRUE);
                      }

                      ready_ = true;
                      initializing_ = false;
                      Logger::info("WebView2BrowserHost: controller ready "
                                   + std::to_string(width_) + "x" + std::to_string(height_));

                      if (!currentUrl_.isEmpty()) {
                          lastNavigatedUrl_.clear();
                          navigate(currentUrl_);
                      }
                      return S_OK;
                  })
                  .Get());
#endif
}

void WebView2BrowserHost::navigate(const QString& url) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    const QString normalized = normalizeBrowserUrl(url);
    currentUrl_ = normalized;
    if (normalized.isEmpty()) {
        return;
    }

    if (!ready_ || !webView_) {
        ensureInitialized(width_, height_);
        return;
    }

    if (normalized == lastNavigatedUrl_ && pageReady_ && !navigationPending_) {
        return;
    }

    lastNavigatedUrl_ = normalized;
    navigationPending_ = true;
    pageReady_ = false;
    Logger::info("WebView2BrowserHost: navigate " + normalized.toStdString());
    static_cast<ICoreWebView2*>(webView_)->Navigate(normalized.toStdWString().c_str());
#else
    Q_UNUSED(url);
#endif
}

void WebView2BrowserHost::reload() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!ready_ || !webView_) {
        if (!currentUrl_.isEmpty()) {
            lastNavigatedUrl_.clear();
            navigate(currentUrl_);
        }
        return;
    }
    navigationPending_ = true;
    pageReady_ = false;
    lastNavigatedUrl_.clear();
    static_cast<ICoreWebView2*>(webView_)->Reload();
#endif
}

void WebView2BrowserHost::forceNavigate(const QString& url) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    lastNavigatedUrl_.clear();
    pageReady_ = false;
    navigate(url);
#else
    Q_UNUSED(url);
#endif
}

void WebView2BrowserHost::capture(std::function<void(QImage)> callback) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!callback) {
        return;
    }
    pendingCapture_ = std::move(callback);

    if (!available_) {
        auto cb = std::move(pendingCapture_);
        pendingCapture_ = nullptr;
        cb({});
        return;
    }
    if (!ready_) {
        ensureInitialized(width_, height_);
        return;
    }
    if (navigationPending_) {
        return;
    }
    auto cb = std::move(pendingCapture_);
    pendingCapture_ = nullptr;
    performCapture(std::move(cb));
#else
    if (callback) {
        callback({});
    }
#endif
}

void WebView2BrowserHost::startCaptureLoop(int intervalMs,
                                           std::function<void(QImage)> callback) {
    loopCaptureCallback_ = std::move(callback);
    if (!captureTimer_) {
        captureTimer_ = new QTimer(this);
        connect(captureTimer_, &QTimer::timeout, this, [this]() {
            if (!loopCaptureCallback_ || !pageReady_ || navigationPending_ || captureBusy_) {
                return;
            }
            capture(loopCaptureCallback_);
        });
    }
    captureTimer_->setInterval(std::max(16, intervalMs));
    if (!captureTimer_->isActive()) {
        captureTimer_->start();
    } else {
        // Restart so the new interval applies immediately.
        captureTimer_->start();
    }
}

void WebView2BrowserHost::stopCaptureLoop() {
    if (captureTimer_) {
        captureTimer_->stop();
    }
    loopCaptureCallback_ = nullptr;
}

void WebView2BrowserHost::onNavigationCompleted() {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (hostWidget_) {
        placeHostForCapture(reinterpret_cast<HWND>(hostWidget_->winId()), width_, height_);
    }
    if (controller_) {
        auto* controller = static_cast<ICoreWebView2Controller*>(controller_);
        controller->put_IsVisible(TRUE);
        controller->NotifyParentWindowPositionChanged();
    }
    if (pendingCapture_) {
        auto cb = std::move(pendingCapture_);
        pendingCapture_ = nullptr;
        performCapture(std::move(cb));
    } else if (loopCaptureCallback_) {
        performCapture(loopCaptureCallback_);
    }
#endif
}

void WebView2BrowserHost::deliverCapture(std::function<void(QImage)> callback, QImage image) {
    captureBusy_ = false;
    if (!callback) {
        return;
    }
    static int s_loggedFrames = 0;
    if (!image.isNull()) {
        if (s_loggedFrames < 5) {
            Logger::info("WebView2BrowserHost: frame "
                         + std::to_string(image.width()) + "x" + std::to_string(image.height()));
            ++s_loggedFrames;
        }
    } else if (s_loggedFrames < 8) {
        Logger::warn("WebView2BrowserHost: empty capture frame");
        ++s_loggedFrames;
    }
    callback(std::move(image));
}

void WebView2BrowserHost::performCapture(std::function<void(QImage)> callback) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!callback || !webView_ || captureBusy_) {
        return;
    }
    // CDP reads the browser compositor directly — reliable for Scoreholio-style overlays.
    // CapturePreview is only a fallback (it often returns solid gray for HWND hosts).
    performCaptureCdp(std::move(callback), /*allowPreviewFallback=*/true);
#else
    Q_UNUSED(callback);
#endif
}

void WebView2BrowserHost::performCaptureCdp(std::function<void(QImage)> callback,
                                            bool allowPreviewFallback) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!callback || !webView_) {
        return;
    }
    if (captureBusy_) {
        return;
    }
    captureBusy_ = true;
    auto* webview = static_cast<ICoreWebView2*>(webView_);

    webview->CallDevToolsProtocolMethod(
        L"Page.captureScreenshot",
        L"{\"format\":\"png\",\"fromSurface\":true,\"captureBeyondViewport\":false}",
        Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
            [this, allowPreviewFallback, callback = std::move(callback)](HRESULT result,
                                                                         LPCWSTR returnJson) -> HRESULT {
                QImage image;
                if (SUCCEEDED(result)) {
                    image = decodeCdpPng(returnJson);
                }

                if (!image.isNull() && !isUselessCapture(image)) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, callback = std::move(callback), image = std::move(image)]() mutable {
                            deliverCapture(std::move(callback), std::move(image));
                        },
                        Qt::QueuedConnection);
                    return S_OK;
                }

                if (allowPreviewFallback) {
                    QMetaObject::invokeMethod(
                        this,
                        [this, callback = std::move(callback)]() mutable {
                            captureBusy_ = false;
                            performCapturePreview(std::move(callback));
                        },
                        Qt::QueuedConnection);
                    return S_OK;
                }

                QMetaObject::invokeMethod(
                    this,
                    [this, callback = std::move(callback)]() mutable {
                        deliverCapture(std::move(callback), {});
                    },
                    Qt::QueuedConnection);
                return S_OK;
            })
            .Get());
#else
    Q_UNUSED(callback);
    Q_UNUSED(allowPreviewFallback);
#endif
}

void WebView2BrowserHost::performCapturePreview(std::function<void(QImage)> callback) {
#ifdef RAILSHOT_HAS_WEBVIEW2
    if (!callback || !webView_) {
        return;
    }
    if (captureBusy_) {
        return;
    }
    captureBusy_ = true;

    if (hostWidget_) {
        placeHostForCapture(reinterpret_cast<HWND>(hostWidget_->winId()), width_, height_);
    }
    if (controller_) {
        auto* controller = static_cast<ICoreWebView2Controller*>(controller_);
        controller->put_IsVisible(TRUE);
        controller->NotifyParentWindowPositionChanged();
    }

    auto* webview = static_cast<ICoreWebView2*>(webView_);
    ComPtr<IStream> pngStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &pngStream))) {
        deliverCapture(std::move(callback), {});
        return;
    }

    webview->CapturePreview(
        COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG, pngStream.Get(),
        Callback<ICoreWebView2CapturePreviewCompletedHandler>(
            [this, callback = std::move(callback), pngStream](HRESULT result) -> HRESULT {
                QImage image;
                if (SUCCEEDED(result)) {
                    STATSTG stats{};
                    if (SUCCEEDED(pngStream->Stat(&stats, STATFLAG_NONAME))) {
                        const auto size = static_cast<size_t>(stats.cbSize.QuadPart);
                        std::vector<BYTE> buffer(size);
                        ULONG read = 0;
                        LARGE_INTEGER zero{};
                        pngStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                        if (SUCCEEDED(pngStream->Read(buffer.data(), static_cast<ULONG>(size),
                                                      &read))
                            && read > 0) {
                            image = QImage::fromData(buffer.data(), static_cast<int>(read), "PNG");
                        }
                    }
                } else {
                    Logger::warn("WebView2BrowserHost: CapturePreview HRESULT failed");
                }

                if (!image.isNull() && isUselessCapture(image)) {
                    Logger::warn("WebView2BrowserHost: CapturePreview solid/empty frame discarded");
                    image = {};
                }

                QMetaObject::invokeMethod(
                    this,
                    [this, callback = std::move(callback), image = std::move(image)]() mutable {
                        deliverCapture(std::move(callback), std::move(image));
                    },
                    Qt::QueuedConnection);
                return S_OK;
            })
            .Get());
#else
    Q_UNUSED(callback);
#endif
}

} // namespace railshot
