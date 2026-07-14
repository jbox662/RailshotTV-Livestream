#include "capture/WasapiAudioMonitor.h"

#include "core/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace railshot {
namespace {

constexpr GUID kIeeeFloatSubformat = {
    0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}};

bool isFloatFormat(const WAVEFORMATEX* format) {
    if (!format) {
        return false;
    }
    if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        return true;
    }
    if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE && format->cbSize >= 22) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
        return IsEqualGUID(ext->SubFormat, kIeeeFloatSubformat);
    }
    return false;
}

} // namespace

WasapiAudioMonitor::WasapiAudioMonitor() = default;

WasapiAudioMonitor::~WasapiAudioMonitor() {
    close();
}

bool WasapiAudioMonitor::open(const std::string& deviceId) {
    std::lock_guard lock(mutex_);
    releaseResources();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comInit && FAILED(hr)) {
        Logger::error("WASAPI monitor: COM init failed");
        return false;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        Logger::error("WASAPI monitor: enumerator failed");
        return false;
    }

    IMMDevice* device = nullptr;
    if (!deviceId.empty()) {
        const int wlen = MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, nullptr, 0);
        std::wstring wide(static_cast<size_t>(std::max(wlen - 1, 0)), L'\0');
        if (wlen > 1) {
            MultiByteToWideChar(CP_UTF8, 0, deviceId.c_str(), -1, wide.data(), wlen);
        }
        hr = enumerator->GetDevice(wide.c_str(), &device);
        if (FAILED(hr)) {
            device = nullptr;
        }
    }
    if (!device) {
        hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    }
    enumerator->Release();
    if (FAILED(hr) || !device) {
        Logger::error("WASAPI monitor: no render endpoint");
        return false;
    }

    IAudioClient* client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    device->Release();
    if (FAILED(hr)) {
        Logger::error("WASAPI monitor: activate failed");
        return false;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = client->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
        client->Release();
        return false;
    }

    deviceRate_ = static_cast<int>(mixFormat->nSamplesPerSec);
    deviceChannels_ = static_cast<int>(mixFormat->nChannels);
    deviceFloat_ = isFloatFormat(mixFormat);

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, mixFormat, nullptr);
    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) {
        Logger::error("WASAPI monitor: initialize failed");
        client->Release();
        return false;
    }

    hr = client->GetBufferSize(&bufferFrames_);
    if (FAILED(hr)) {
        client->Release();
        return false;
    }

    IAudioRenderClient* render = nullptr;
    hr = client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&render));
    if (FAILED(hr)) {
        client->Release();
        return false;
    }

    hr = client->Start();
    if (FAILED(hr)) {
        render->Release();
        client->Release();
        return false;
    }

    audioClient_ = client;
    renderClient_ = render;
    pending_.clear();
    Logger::info("WASAPI monitor: opened render " + std::to_string(deviceRate_) + " Hz / "
                 + std::to_string(deviceChannels_) + " ch");
    return true;
}

void WasapiAudioMonitor::close() {
    std::lock_guard lock(mutex_);
    releaseResources();
    enabled_ = false;
}

void WasapiAudioMonitor::releaseResources() {
    if (audioClient_) {
        audioClient_->Stop();
    }
    if (renderClient_) {
        renderClient_->Release();
        renderClient_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }
    pending_.clear();
    bufferFrames_ = 0;
}

void WasapiAudioMonitor::setEnabled(bool enabled) {
    enabled_.store(enabled);
}

void WasapiAudioMonitor::write(const AudioFrame& frame) {
    if (!enabled_.load() || !frame.isValid()) {
        return;
    }

    std::lock_guard lock(mutex_);
    if (!audioClient_ || !renderClient_) {
        return;
    }

    // Append incoming S16 stereo @ 48k into pending.
    const auto* in = reinterpret_cast<const int16_t*>(frame.data.data());
    const size_t inSamples = frame.data.size() / sizeof(int16_t);
    pending_.insert(pending_.end(), in, in + inSamples);

    UINT32 padding = 0;
    if (FAILED(audioClient_->GetCurrentPadding(&padding))) {
        return;
    }
    const UINT32 available = bufferFrames_ > padding ? bufferFrames_ - padding : 0;
    if (available == 0) {
        // Drop oldest if backlog grows huge (>0.5s @ 48k stereo).
        const size_t maxPending = 48000 * 2 / 2;
        if (pending_.size() > maxPending) {
            pending_.erase(pending_.begin(), pending_.begin() + (pending_.size() - maxPending));
        }
        return;
    }

    const double ratio = static_cast<double>(deviceRate_) / 48000.0;
    const size_t pendingFrames = pending_.size() / 2;
    const UINT32 wantFrames =
        static_cast<UINT32>(std::min(static_cast<size_t>(available),
                                     static_cast<size_t>(std::max(1.0, pendingFrames * ratio))));
    if (wantFrames == 0 || pendingFrames == 0) {
        return;
    }

    BYTE* buffer = nullptr;
    if (FAILED(renderClient_->GetBuffer(wantFrames, &buffer)) || !buffer) {
        return;
    }

    for (UINT32 f = 0; f < wantFrames; ++f) {
        const double srcPos = static_cast<double>(f) / ratio;
        const size_t i0 = std::min(static_cast<size_t>(srcPos), pendingFrames - 1);
        const size_t i1 = std::min(i0 + 1, pendingFrames - 1);
        const float frac = static_cast<float>(srcPos - static_cast<double>(i0));

        for (int ch = 0; ch < deviceChannels_; ++ch) {
            const int srcCh = std::min(ch, 1);
            const float s0 = static_cast<float>(pending_[i0 * 2 + srcCh]) / 32768.0f;
            const float s1 = static_cast<float>(pending_[i1 * 2 + srcCh]) / 32768.0f;
            const float s = s0 + (s1 - s0) * frac;

            if (deviceFloat_) {
                reinterpret_cast<float*>(buffer)[f * deviceChannels_ + ch] = s;
            } else {
                reinterpret_cast<int16_t*>(buffer)[f * deviceChannels_ + ch] =
                    static_cast<int16_t>(std::clamp(static_cast<int>(s * 32767.0f), -32768, 32767));
            }
        }
    }

    renderClient_->ReleaseBuffer(wantFrames, 0);

    const size_t consumedFrames =
        std::min(pendingFrames, static_cast<size_t>(std::lround(wantFrames / ratio)));
    if (consumedFrames > 0) {
        pending_.erase(pending_.begin(),
                       pending_.begin() + static_cast<std::ptrdiff_t>(consumedFrames * 2));
    }
}

} // namespace railshot
