#include "capture/WgcWindowCapture.h"

#include "capture/CaptureConvert.h"
#include "core/Logger.h"

#include <windows.h>

#include <DispatcherQueue.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <chrono>
#include <mutex>
#include <vector>

#pragma comment(lib, "windowsapp")

namespace railshot {
namespace {

winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice
createWinrtDevice(ID3D11Device* d3dDevice) {
    winrt::com_ptr<IDXGIDevice> dxgiDevice;
    winrt::check_hresult(d3dDevice->QueryInterface(dxgiDevice.put()));
    winrt::com_ptr<::IInspectable> inspectable;
    winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
    return inspectable.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

winrt::Windows::Graphics::Capture::GraphicsCaptureItem createItemForWindow(HWND hwnd) {
    auto interopFactory = winrt::get_activation_factory<
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem item{nullptr};
    winrt::check_hresult(interopFactory->CreateForWindow(
        hwnd, winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        reinterpret_cast<void**>(winrt::put_abi(item))));
    return item;
}

ID3D11Texture2D* getTextureFromSurface(
    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface const& surface) {
    auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    ID3D11Texture2D* texture = nullptr;
    winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(&texture)));
    return texture;
}

} // namespace

bool WgcWindowCapture::isSupported() {
    try {
        return winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
    } catch (...) {
        return false;
    }
}

WgcWindowCapture::WgcWindowCapture() = default;

WgcWindowCapture::~WgcWindowCapture() {
    stop();
    close();
}

bool WgcWindowCapture::open(uintptr_t hwnd) {
    close();
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h)) {
        Logger::error("WGC: invalid HWND");
        return false;
    }
    if (!isSupported()) {
        Logger::error("WGC: Graphics Capture not supported on this OS");
        return false;
    }
    hwnd_ = hwnd;
    return true;
}

void WgcWindowCapture::close() {
    stop();
    hwnd_ = 0;
}

bool WgcWindowCapture::start(ThreadSafeQueue<VideoFrame>* outputQueue) {
    if (running_.load() || !outputQueue || hwnd_ == 0) {
        return false;
    }
    outputQueue_ = outputQueue;
    stopRequested_ = false;
    running_ = true;
    captureThread_ = std::make_unique<std::thread>(&WgcWindowCapture::captureThreadFunc, this);
    return true;
}

void WgcWindowCapture::stop() {
    stopRequested_ = true;
    if (captureThread_ && captureThread_->joinable()) {
        captureThread_->join();
    }
    captureThread_.reset();
    running_ = false;
    outputQueue_ = nullptr;
}

void WgcWindowCapture::captureThreadFunc() {
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);
    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL level{};
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                               D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                               D3D11_SDK_VERSION, device.put(), &level,
                                               context.put()));

        auto winrtDevice = createWinrtDevice(device.get());
        auto item = createItemForWindow(hwnd);
        const auto size = item.Size();
        if (size.Width < 2 || size.Height < 2) {
            Logger::error("WGC: capture item has empty size");
            running_ = false;
            return;
        }

        auto framePool = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
            winrtDevice,
            winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
            size);
        auto session = framePool.CreateCaptureSession(item);
        session.IsBorderRequired(false);
        session.StartCapture();

        winrt::com_ptr<ID3D11Texture2D> staging;
        int stagingW = 0;
        int stagingH = 0;
        int poolW = size.Width;
        int poolH = size.Height;

        Logger::info("WGC: capturing HWND " + std::to_string(hwnd_));

        while (!stopRequested_.load()) {
            if (!IsWindow(hwnd)) {
                break;
            }

            auto frame = framePool.TryGetNextFrame();
            if (!frame) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                continue;
            }

            auto contentSize = frame.ContentSize();
            if (contentSize.Width != poolW || contentSize.Height != poolH) {
                framePool.Recreate(
                    winrtDevice,
                    winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2,
                    contentSize);
                poolW = contentSize.Width;
                poolH = contentSize.Height;
            }

            auto surface = frame.Surface();
            winrt::com_ptr<ID3D11Texture2D> texture;
            texture.attach(getTextureFromSurface(surface));
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            const int w = static_cast<int>(desc.Width);
            const int h = static_cast<int>(desc.Height);
            if (w < 2 || h < 2) {
                continue;
            }

            if (!staging || stagingW != w || stagingH != h) {
                staging = nullptr;
                D3D11_TEXTURE2D_DESC stagingDesc = desc;
                stagingDesc.Usage = D3D11_USAGE_STAGING;
                stagingDesc.BindFlags = 0;
                stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                stagingDesc.MiscFlags = 0;
                winrt::check_hresult(device->CreateTexture2D(&stagingDesc, nullptr, staging.put()));
                stagingW = w;
                stagingH = h;
            }

            context->CopyResource(staging.get(), texture.get());
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped))) {
                continue;
            }

            VideoFrame out;
            bgraToNv12(static_cast<const uint8_t*>(mapped.pData), w, h,
                       static_cast<int>(mapped.RowPitch), out);
            context->Unmap(staging.get(), 0);

            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            out.timestampUs = std::chrono::duration_cast<std::chrono::microseconds>(now).count();
            if (outputQueue_ && out.isValid()) {
                outputQueue_->push(std::move(out));
            }
        }

        session.Close();
        framePool.Close();
    } catch (const winrt::hresult_error& e) {
        Logger::error("WGC: exception " + winrt::to_string(e.message()));
    } catch (...) {
        Logger::error("WGC: unknown exception");
    }
    running_ = false;
}

} // namespace railshot
