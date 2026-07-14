#include "capture/DirectShowCapture.h"

#include "core/Logger.h"

#include <dshow.h>
#include <dvdmedia.h>
#include "capture/qedit.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#pragma comment(lib, "strmiids.lib")

namespace railshot {

namespace {

std::string wideToUtf8(const wchar_t* wide) {
    if (!wide) {
        return {};
    }
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) {
        return {};
    }
    std::string result(static_cast<size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], len, nullptr, nullptr);
    return result;
}

std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len <= 0) {
        return {};
    }
    std::wstring result(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &result[0], len);
    return result;
}

void deleteMediaType(AM_MEDIA_TYPE* pmt) {
    if (!pmt) {
        return;
    }
    if (pmt->cbFormat != 0) {
        CoTaskMemFree(pmt->pbFormat);
    }
    if (pmt->pUnk) {
        pmt->pUnk->Release();
    }
    CoTaskMemFree(pmt);
}

PixelFormat formatFromSubtype(const GUID& subtype) {
    if (subtype == MEDIASUBTYPE_NV12) {
        return PixelFormat::NV12;
    }
    if (subtype == MEDIASUBTYPE_YUY2) {
        return PixelFormat::YUY2;
    }
    if (subtype == MEDIASUBTYPE_RGB24) {
        return PixelFormat::RGB24;
    }
    return PixelFormat::NV12;
}

void convertYuy2ToNv12(const uint8_t* src, uint8_t* dst, int width, int height) {
    const int ySize = width * height;
    auto* yPlane = dst;
    auto* uvPlane = dst + ySize;

    for (int row = 0; row < height; ++row) {
        const auto* rowSrc = src + row * width * 2;
        for (int col = 0; col < width; col += 2) {
            const int y0 = rowSrc[col * 2 + 0];
            const int u  = rowSrc[col * 2 + 1];
            const int y1 = rowSrc[col * 2 + 2];
            const int v  = rowSrc[col * 2 + 3];

            yPlane[row * width + col]     = static_cast<uint8_t>(y0);
            yPlane[row * width + col + 1] = static_cast<uint8_t>(y1);

            const int uvIndex = (row / 2) * width + col;
            uvPlane[uvIndex]     = static_cast<uint8_t>(u);
            uvPlane[uvIndex + 1] = static_cast<uint8_t>(v);
        }
    }
}

void convertRgb24ToNv12(const uint8_t* src, uint8_t* dst, int width, int height) {
    const int ySize = width * height;
    auto* yPlane = dst;
    auto* uvPlane = dst + ySize;

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const int idx = (row * width + col) * 3;
            const int r = src[idx + 2];
            const int g = src[idx + 1];
            const int b = src[idx + 0];

            const int y = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yPlane[row * width + col] = static_cast<uint8_t>(std::clamp(y, 0, 255));

            if ((row & 1) == 0 && (col & 1) == 0) {
                const int u = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
                const int v = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;
                const int uvIndex = (row / 2) * width + col;
                uvPlane[uvIndex]     = static_cast<uint8_t>(std::clamp(u, 0, 255));
                uvPlane[uvIndex + 1] = static_cast<uint8_t>(std::clamp(v, 0, 255));
            }
        }
    }
}

class SampleGrabberCallback : public ISampleGrabberCB {
public:
    SampleGrabberCallback(DirectShowCapture* owner,
                          ThreadSafeQueue<VideoFrame>* queue,
                          PixelFormat format,
                          int width,
                          int height)
        : owner_(owner)
        , queue_(queue)
        , format_(format)
        , width_(width)
        , height_(height) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == IID_ISampleGrabberCB || riid == IID_IUnknown) {
            *ppv = static_cast<ISampleGrabberCB*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refCount_); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG count = InterlockedDecrement(&refCount_);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    STDMETHODIMP SampleCB(double /*time*/, IMediaSample* /*sample*/) override {
        return S_OK;
    }

    STDMETHODIMP BufferCB(double /*time*/, BYTE* buffer, long len) override {
        if (!queue_ || !buffer || len <= 0) {
            return S_OK;
        }

        VideoFrame frame;
        frame.width = width_;
        frame.height = height_;
        frame.format = PixelFormat::NV12;
        frame.allocate(width_, height_, PixelFormat::NV12);

        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        frame.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();

        if (format_ == PixelFormat::NV12) {
            const size_t copySize = std::min(frame.data.size(), static_cast<size_t>(len));
            std::memcpy(frame.data.data(), buffer, copySize);
        } else if (format_ == PixelFormat::YUY2) {
            convertYuy2ToNv12(buffer, frame.data.data(), width_, height_);
        } else {
            convertRgb24ToNv12(buffer, frame.data.data(), width_, height_);
        }

        queue_->push(std::move(frame));
        return S_OK;
    }

private:
    DirectShowCapture* owner_;
    ThreadSafeQueue<VideoFrame>* queue_;
    PixelFormat format_;
    int width_;
    int height_;
    LONG refCount_ = 1;
};

IBaseFilter* findCaptureFilter(const std::string& deviceId) {
    ICreateDevEnum* devEnum = nullptr;
    IEnumMoniker* enumMoniker = nullptr;
    IMoniker* moniker = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum,
                                  reinterpret_cast<void**>(&devEnum));
    if (FAILED(hr)) {
        return nullptr;
    }

    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (FAILED(hr) || !enumMoniker) {
        devEnum->Release();
        return nullptr;
    }

    IBaseFilter* result = nullptr;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        LPOLESTR displayName = nullptr;
        moniker->GetDisplayName(nullptr, nullptr, &displayName);
        const std::string id = wideToUtf8(displayName);
        CoTaskMemFree(displayName);

        if (deviceId.empty() || id == deviceId) {
            hr = moniker->BindToObject(nullptr, nullptr, IID_IBaseFilter,
                                       reinterpret_cast<void**>(&result));
            moniker->Release();
            break;
        }
        moniker->Release();
    }

    enumMoniker->Release();
    devEnum->Release();
    return result;
}

bool configureCapturePin(IBaseFilter* captureFilter, int& width, int& height, GUID& subtype) {
    IAMStreamConfig* streamConfig = nullptr;
    HRESULT hr = captureFilter->QueryInterface(IID_IAMStreamConfig,
                                               reinterpret_cast<void**>(&streamConfig));
    if (FAILED(hr) || !streamConfig) {
        return false;
    }

    int count = 0;
    int size = 0;
    hr = streamConfig->GetNumberOfCapabilities(&count, &size);
    if (FAILED(hr) || count == 0) {
        streamConfig->Release();
        return false;
    }

    auto* caps = reinterpret_cast<VIDEO_STREAM_CONFIG_CAPS*>(
        CoTaskMemAlloc(size));
    if (!caps) {
        streamConfig->Release();
        return false;
    }

    AM_MEDIA_TYPE* bestMediaType = nullptr;
    int bestScore = -1;

    for (int i = 0; i < count; ++i) {
        AM_MEDIA_TYPE* mediaType = nullptr;
        if (FAILED(streamConfig->GetStreamCaps(i, &mediaType, reinterpret_cast<BYTE*>(caps)))) {
            continue;
        }

        if (mediaType->formattype != FORMAT_VideoInfo) {
            continue;
        }

        const auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(mediaType->pbFormat);
        const int w = vih->bmiHeader.biWidth;
        const int h = std::abs(vih->bmiHeader.biHeight);

        int score = 0;
        if (mediaType->subtype == MEDIASUBTYPE_NV12)  score += 100;
        if (mediaType->subtype == MEDIASUBTYPE_YUY2)  score += 80;
        if (mediaType->subtype == MEDIASUBTYPE_RGB24) score += 40;
        if (w == 1920 && h == 1080) score += 50;
        if (w >= 1280) score += 10;

        if (score > bestScore) {
            if (bestMediaType) {
                deleteMediaType(bestMediaType);
            }
            bestMediaType = mediaType;
            bestScore = score;
            mediaType = nullptr;
        }

        if (mediaType) {
            deleteMediaType(mediaType);
        }
    }

    CoTaskMemFree(caps);

    if (!bestMediaType) {
        streamConfig->Release();
        return false;
    }

    hr = streamConfig->SetFormat(bestMediaType);
  const auto* vih = reinterpret_cast<VIDEOINFOHEADER*>(bestMediaType->pbFormat);
    width = vih->bmiHeader.biWidth;
    height = std::abs(vih->bmiHeader.biHeight);
    subtype = bestMediaType->subtype;

    deleteMediaType(bestMediaType);
    streamConfig->Release();
    return SUCCEEDED(hr);
}

} // namespace

DirectShowCapture::DirectShowCapture() = default;

DirectShowCapture::~DirectShowCapture() {
    stop();
    close();
}

std::vector<CaptureDevice> DirectShowCapture::enumerateDevices() {
    std::vector<CaptureDevice> devices;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInit = SUCCEEDED(hr);

    ICreateDevEnum* devEnum = nullptr;
    IEnumMoniker* enumMoniker = nullptr;

    hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ICreateDevEnum, reinterpret_cast<void**>(&devEnum));
    if (FAILED(hr)) {
        if (comInit) CoUninitialize();
        return devices;
    }

    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (SUCCEEDED(hr) && enumMoniker) {
        IMoniker* moniker = nullptr;
        while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
            IPropertyBag* bag = nullptr;
            if (SUCCEEDED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                                 reinterpret_cast<void**>(&bag)))) {
                VARIANT varName;
                VariantInit(&varName);
                if (SUCCEEDED(bag->Read(L"FriendlyName", &varName, nullptr))) {
                    LPOLESTR displayName = nullptr;
                    moniker->GetDisplayName(nullptr, nullptr, &displayName);

                    CaptureDevice dev;
                    dev.id = wideToUtf8(displayName);
                    dev.name = wideToUtf8(varName.bstrVal);
                    devices.push_back(std::move(dev));

                    CoTaskMemFree(displayName);
                }
                VariantClear(&varName);
                bag->Release();
            }
            moniker->Release();
        }
        enumMoniker->Release();
    }

    devEnum->Release();
    if (comInit) CoUninitialize();
    return devices;
}

bool DirectShowCapture::open(const std::string& deviceId) {
    close();
    deviceId_ = deviceId;
    return true;
}

void DirectShowCapture::close() {
    stop();
    releaseGraph();
    deviceId_.clear();
    captureWidth_ = 0;
    captureHeight_ = 0;
}

bool DirectShowCapture::start(ThreadSafeQueue<VideoFrame>* outputQueue) {
    if (running_.load() || !outputQueue) {
        return false;
    }

    outputQueue_ = outputQueue;
    stopRequested_ = false;
    running_ = true;

    captureThread_ = std::make_unique<std::thread>(&DirectShowCapture::captureThreadFunc, this);
    return true;
}

void DirectShowCapture::stop() {
    stopRequested_ = true;
    if (mediaControl_) {
        mediaControl_->Stop();
    }
    if (captureThread_ && captureThread_->joinable()) {
        captureThread_->join();
    }
    captureThread_.reset();
    releaseGraph();
    running_ = false;
    outputQueue_ = nullptr;
}

void DirectShowCapture::releaseGraph() {
    if (mediaControl_) {
        mediaControl_->Release();
        mediaControl_ = nullptr;
    }
    if (grabberCallback_) {
        grabberCallback_->Release();
        grabberCallback_ = nullptr;
    }
    if (sampleGrabber_) {
        sampleGrabber_->Release();
        sampleGrabber_ = nullptr;
    }
    if (grabberFilter_) {
        grabberFilter_->Release();
        grabberFilter_ = nullptr;
    }
    if (captureFilter_) {
        captureFilter_->Release();
        captureFilter_ = nullptr;
    }
    if (captureBuilder_) {
        captureBuilder_->Release();
        captureBuilder_ = nullptr;
    }
    if (graph_) {
        graph_->Release();
        graph_ = nullptr;
    }
}

bool DirectShowCapture::buildGraph() {
    releaseGraph();

    HRESULT hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IGraphBuilder, reinterpret_cast<void**>(&graph_));
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to create FilterGraph");
        return false;
    }

    hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, nullptr, CLSCTX_INPROC_SERVER,
                          IID_ICaptureGraphBuilder2,
                          reinterpret_cast<void**>(&captureBuilder_));
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to create CaptureGraphBuilder2");
        return false;
    }

    captureBuilder_->SetFiltergraph(graph_);

    captureFilter_ = findCaptureFilter(deviceId_);
    if (!captureFilter_) {
        Logger::error("DirectShow: capture device not found");
        return false;
    }

    hr = graph_->AddFilter(captureFilter_, L"Video Capture");
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to add capture filter");
        return false;
    }

    GUID subtype = MEDIASUBTYPE_NV12;
    if (!configureCapturePin(captureFilter_, captureWidth_, captureHeight_, subtype)) {
        Logger::warn("DirectShow: could not configure preferred format, using defaults");
        captureWidth_ = 1920;
        captureHeight_ = 1080;
        subtype = MEDIASUBTYPE_YUY2;
    }
    captureFormat_ = formatFromSubtype(subtype);

    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, reinterpret_cast<void**>(&grabberFilter_));
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to create SampleGrabber");
        return false;
    }

    hr = graph_->AddFilter(grabberFilter_, L"Sample Grabber");
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to add SampleGrabber");
        return false;
    }

    hr = grabberFilter_->QueryInterface(IID_ISampleGrabber,
                                        reinterpret_cast<void**>(&sampleGrabber_));
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to get ISampleGrabber");
        return false;
    }

    AM_MEDIA_TYPE mediaType{};
    mediaType.majortype = MEDIATYPE_Video;
    mediaType.subtype = subtype;
    mediaType.formattype = FORMAT_VideoInfo;
    hr = sampleGrabber_->SetMediaType(&mediaType);
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to set grabber media type");
        return false;
    }

    sampleGrabber_->SetBufferSamples(FALSE);
    sampleGrabber_->SetOneShot(FALSE);

    grabberCallback_ = new SampleGrabberCallback(this, outputQueue_, captureFormat_,
                                                 captureWidth_, captureHeight_);
    hr = sampleGrabber_->SetCallback(grabberCallback_, 1);
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to set grabber callback");
        return false;
    }

    hr = captureBuilder_->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                                      captureFilter_, nullptr, grabberFilter_);
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to render capture stream");
        return false;
    }

    hr = graph_->QueryInterface(IID_IMediaControl, reinterpret_cast<void**>(&mediaControl_));
    if (FAILED(hr)) {
        Logger::error("DirectShow: failed to get IMediaControl");
        return false;
    }

    Logger::info("DirectShow: opened device at " +
                 std::to_string(captureWidth_) + "x" + std::to_string(captureHeight_));
    return true;
}

void DirectShowCapture::captureThreadFunc() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        Logger::error("DirectShow: COM init failed on capture thread");
        running_ = false;
        return;
    }

    if (!buildGraph()) {
        running_ = false;
        CoUninitialize();
        return;
    }

    const HRESULT runHr = mediaControl_->Run();
    if (FAILED(runHr)) {
        Logger::error("DirectShow: failed to start graph");
        running_ = false;
        releaseGraph();
        CoUninitialize();
        return;
    }

    while (!stopRequested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (mediaControl_) {
        mediaControl_->Stop();
    }
    releaseGraph();
    CoUninitialize();
    running_ = false;
}

} // namespace railshot
