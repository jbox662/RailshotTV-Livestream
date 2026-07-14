#include "capture/DxgiMonitorCapture.h"

#include "capture/CaptureConvert.h"
#include "core/Logger.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#include <chrono>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace railshot {

DxgiMonitorCapture::DxgiMonitorCapture() = default;

DxgiMonitorCapture::~DxgiMonitorCapture() {
    stop();
    close();
}

std::vector<MonitorInfo> DxgiMonitorCapture::enumerateMonitors() {
    std::vector<MonitorInfo> monitors;
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))
        || !factory) {
        return monitors;
    }

    int globalIndex = 0;
    for (UINT adapterIndex = 0;; ++adapterIndex) {
        IDXGIAdapter1* adapter = nullptr;
        if (factory->EnumAdapters1(adapterIndex, &adapter) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        for (UINT outputIndex = 0;; ++outputIndex) {
            IDXGIOutput* output = nullptr;
            if (adapter->EnumOutputs(outputIndex, &output) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            DXGI_OUTPUT_DESC desc{};
            if (SUCCEEDED(output->GetDesc(&desc))) {
                MonitorInfo info;
                info.index = globalIndex++;
                const auto* name = desc.DeviceName;
                info.name = std::string(name, name + wcslen(name));
                info.width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
                info.height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
                monitors.push_back(info);
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return monitors;
}

bool DxgiMonitorCapture::open(int monitorIndex) {
    close();
    monitorIndex_ = monitorIndex;

    IDXGIFactory1* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
    if (FAILED(hr) || !factory) {
        Logger::error("DXGI: CreateDXGIFactory1 failed");
        return false;
    }

    // Resolve monitorIndex across all adapters; keep the owning adapter for CreateDevice.
    IDXGIAdapter1* chosenAdapter = nullptr;
    IDXGIOutput* chosenOutput = nullptr;
    int remaining = monitorIndex_;
    for (UINT a = 0; !chosenOutput; ++a) {
        IDXGIAdapter1* ad = nullptr;
        if (factory->EnumAdapters1(a, &ad) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        for (UINT o = 0;; ++o) {
            IDXGIOutput* out = nullptr;
            if (ad->EnumOutputs(o, &out) == DXGI_ERROR_NOT_FOUND) {
                break;
            }
            if (remaining == 0) {
                chosenAdapter = ad;
                chosenOutput = out;
                break;
            }
            out->Release();
            --remaining;
        }
        if (!chosenOutput) {
            ad->Release();
        }
    }
    factory->Release();

    if (!chosenAdapter || !chosenOutput) {
        Logger::error("DXGI: monitor index not found: " + std::to_string(monitorIndex_));
        if (chosenAdapter) {
            chosenAdapter->Release();
        }
        if (chosenOutput) {
            chosenOutput->Release();
        }
        return false;
    }

    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    hr = D3D11CreateDevice(chosenAdapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, nullptr, 0,
                           D3D11_SDK_VERSION, &device_, &level, &context_);
    chosenAdapter->Release();
    if (FAILED(hr) || !device_ || !context_) {
        Logger::error("DXGI: failed to create D3D11 device");
        chosenOutput->Release();
        releaseResources();
        return false;
    }

    IDXGIOutput1* output1 = nullptr;
    hr = chosenOutput->QueryInterface(__uuidof(IDXGIOutput1), reinterpret_cast<void**>(&output1));
    chosenOutput->Release();
    if (FAILED(hr) || !output1) {
        Logger::error("DXGI: IDXGIOutput1 unavailable");
        releaseResources();
        return false;
    }

    hr = output1->DuplicateOutput(device_, &duplication_);
    output1->Release();
    if (FAILED(hr) || !duplication_) {
        Logger::error("DXGI: DuplicateOutput failed (hr=" + std::to_string(hr) + ")");
        releaseResources();
        return false;
    }

    DXGI_OUTDUPL_DESC duplDesc{};
    duplication_->GetDesc(&duplDesc);
    width_ = static_cast<int>(duplDesc.ModeDesc.Width);
    height_ = static_cast<int>(duplDesc.ModeDesc.Height);

    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = static_cast<UINT>(width_);
    stagingDesc.Height = static_cast<UINT>(height_);
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = device_->CreateTexture2D(&stagingDesc, nullptr, &staging_);
    if (FAILED(hr) || !staging_) {
        Logger::error("DXGI: CreateTexture2D staging failed");
        releaseResources();
        return false;
    }

    Logger::info("DXGI: opened monitor " + std::to_string(monitorIndex_) + " "
                 + std::to_string(width_) + "x" + std::to_string(height_));
    return true;
}

void DxgiMonitorCapture::close() {
    stop();
    releaseResources();
}

void DxgiMonitorCapture::releaseResources() {
    if (staging_) {
        staging_->Release();
        staging_ = nullptr;
    }
    if (duplication_) {
        duplication_->Release();
        duplication_ = nullptr;
    }
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
    if (device_) {
        device_->Release();
        device_ = nullptr;
    }
    width_ = 0;
    height_ = 0;
}

bool DxgiMonitorCapture::start(ThreadSafeQueue<VideoFrame>* outputQueue) {
    if (running_.load() || !outputQueue || !duplication_) {
        return false;
    }
    outputQueue_ = outputQueue;
    stopRequested_ = false;
    running_ = true;
    captureThread_ = std::make_unique<std::thread>(&DxgiMonitorCapture::captureThreadFunc, this);
    return true;
}

void DxgiMonitorCapture::stop() {
    stopRequested_ = true;
    if (captureThread_ && captureThread_->joinable()) {
        captureThread_->join();
    }
    captureThread_.reset();
    running_ = false;
    outputQueue_ = nullptr;
}

void DxgiMonitorCapture::captureThreadFunc() {
    while (!stopRequested_.load()) {
        DXGI_OUTDUPL_FRAME_INFO frameInfo{};
        IDXGIResource* resource = nullptr;
        HRESULT hr = duplication_->AcquireNextFrame(100, &frameInfo, &resource);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
            continue;
        }
        if (FAILED(hr)) {
            Logger::warn("DXGI: AcquireNextFrame failed");
            break;
        }

        ID3D11Texture2D* texture = nullptr;
        hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture));
        resource->Release();
        if (FAILED(hr) || !texture) {
            duplication_->ReleaseFrame();
            continue;
        }

        context_->CopyResource(staging_, texture);
        texture->Release();

        D3D11_MAPPED_SUBRESOURCE mapped{};
        hr = context_->Map(staging_, 0, D3D11_MAP_READ, 0, &mapped);
        if (SUCCEEDED(hr)) {
            VideoFrame frame;
            bgraToNv12(static_cast<const uint8_t*>(mapped.pData), width_, height_,
                       static_cast<int>(mapped.RowPitch), frame);
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            frame.timestampUs =
                std::chrono::duration_cast<std::chrono::microseconds>(now).count();
            if (outputQueue_) {
                outputQueue_->push(std::move(frame));
            }
            context_->Unmap(staging_, 0);
        }

        duplication_->ReleaseFrame();
    }
    running_ = false;
}

} // namespace railshot
