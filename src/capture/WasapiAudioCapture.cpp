#include "capture/WasapiAudioCapture.h"

#include "core/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audioclientactivationparams.h>
#include <mmreg.h>
#include <propidl.h>
#include <functiondiscoverykeys_devpkey.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

namespace railshot {
namespace {

constexpr int kOutRate = 48000;
constexpr int kOutChannels = 2;

// WAVE_FORMAT_IEEE_FLOAT subtype (KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) without ksmedia.h.
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

std::vector<AudioDeviceInfo> enumerateEndpoints(EDataFlow flow) {
    std::vector<AudioDeviceInfo> devices;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comOk = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comOk && FAILED(hr)) {
        return devices;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr) || !enumerator) {
        return devices;
    }

    IMMDevice* defaultDevice = nullptr;
    std::wstring defaultId;
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, eConsole, &defaultDevice))
        && defaultDevice) {
        LPWSTR id = nullptr;
        if (SUCCEEDED(defaultDevice->GetId(&id)) && id) {
            defaultId = id;
            CoTaskMemFree(id);
        }
        defaultDevice->Release();
    }

    IMMDeviceCollection* collection = nullptr;
    hr = enumerator->EnumAudioEndpoints(flow, DEVICE_STATE_ACTIVE, &collection);
    enumerator->Release();
    if (FAILED(hr) || !collection) {
        return devices;
    }

    UINT count = 0;
    collection->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IMMDevice* device = nullptr;
        if (FAILED(collection->Item(i, &device)) || !device) {
            continue;
        }

        AudioDeviceInfo info;
        LPWSTR id = nullptr;
        if (SUCCEEDED(device->GetId(&id)) && id) {
            // Convert wide device id to UTF-8.
            const int bytes = WideCharToMultiByte(CP_UTF8, 0, id, -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(static_cast<size_t>(std::max(bytes - 1, 0)), '\0');
            if (bytes > 1) {
                WideCharToMultiByte(CP_UTF8, 0, id, -1, utf8.data(), bytes, nullptr, nullptr);
            }
            info.id = utf8;
            info.isDefault = (defaultId == id);
            CoTaskMemFree(id);
        }

        IPropertyStore* props = nullptr;
        if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props)) && props) {
            PROPVARIANT var;
            PropVariantInit(&var);
            if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &var)) && var.vt == VT_LPWSTR
                && var.pwszVal) {
                const int bytes =
                    WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, nullptr, 0, nullptr, nullptr);
                std::string utf8(static_cast<size_t>(std::max(bytes - 1, 0)), '\0');
                if (bytes > 1) {
                    WideCharToMultiByte(CP_UTF8, 0, var.pwszVal, -1, utf8.data(), bytes, nullptr,
                                        nullptr);
                }
                info.name = utf8;
            }
            PropVariantClear(&var);
            props->Release();
        }
        device->Release();

        if (info.name.empty()) {
            info.name = info.id.empty() ? "Audio Device" : info.id;
        }
        devices.push_back(std::move(info));
    }
    collection->Release();

    std::sort(devices.begin(), devices.end(),
              [](const AudioDeviceInfo& a, const AudioDeviceInfo& b) {
                  if (a.isDefault != b.isDefault) {
                      return a.isDefault;
                  }
                  return a.name < b.name;
              });
    return devices;
}

float readSampleAsFloat(const uint8_t* base, int frameIdx, int channel, int channels, int bits,
                        bool isFloat) {
    const int ch = std::min(channel, channels - 1);
    if (isFloat) {
        const auto* samples = reinterpret_cast<const float*>(base);
        return samples[frameIdx * channels + ch];
    }
    if (bits == 16) {
        const auto* samples = reinterpret_cast<const int16_t*>(base);
        return static_cast<float>(samples[frameIdx * channels + ch]) / 32768.0f;
    }
    if (bits == 24) {
        const uint8_t* p = base + (frameIdx * channels + ch) * 3;
        int32_t v = (static_cast<int32_t>(p[0])) | (static_cast<int32_t>(p[1]) << 8)
                    | (static_cast<int32_t>(p[2]) << 16);
        if (v & 0x800000) {
            v |= ~0xFFFFFF;
        }
        return static_cast<float>(v) / 8388608.0f;
    }
    if (bits == 32) {
        const auto* samples = reinterpret_cast<const int32_t*>(base);
        return static_cast<float>(samples[frameIdx * channels + ch]) / 2147483648.0f;
    }
    return 0.0f;
}

} // namespace

WasapiAudioCapture::WasapiAudioCapture() = default;

WasapiAudioCapture::~WasapiAudioCapture() {
    stop();
    close();
}

std::vector<AudioDeviceInfo> WasapiAudioCapture::enumerateInputDevices() {
    return enumerateEndpoints(eCapture);
}

std::vector<AudioDeviceInfo> WasapiAudioCapture::enumerateOutputDevices() {
    return enumerateEndpoints(eRender);
}

bool WasapiAudioCapture::openMicrophone(const std::string& deviceId) {
    close();
    return openEndpoint(true, false, deviceId, "WASAPI mic");
}

bool WasapiAudioCapture::openDesktopLoopback(const std::string& deviceId) {
    close();
    return openEndpoint(false, true, deviceId, "WASAPI desktop");
}

bool WasapiAudioCapture::finishClientOpen(IAudioClient* client, const char* label) {
    WAVEFORMATEX* mixFormat = nullptr;
    HRESULT hr = client->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
        Logger::error(std::string(label) + ": failed to get mix format");
        client->Release();
        return false;
    }

    srcSampleRate_ = static_cast<int>(mixFormat->nSamplesPerSec);
    srcChannels_ = static_cast<int>(mixFormat->nChannels);
    srcBits_ = static_cast<int>(mixFormat->wBitsPerSample);
    srcFloat_ = isFloatFormat(mixFormat);

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, mixFormat, nullptr);
    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) {
        // Process loopback clients sometimes want LOOPBACK flag; try once with it.
        hr = client->GetMixFormat(&mixFormat);
        if (SUCCEEDED(hr) && mixFormat) {
            srcSampleRate_ = static_cast<int>(mixFormat->nSamplesPerSec);
            srcChannels_ = static_cast<int>(mixFormat->nChannels);
            srcBits_ = static_cast<int>(mixFormat->wBitsPerSample);
            srcFloat_ = isFloatFormat(mixFormat);
            hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                    10000000, 0, mixFormat, nullptr);
            CoTaskMemFree(mixFormat);
        }
    }
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to initialize audio client");
        client->Release();
        return false;
    }

    IAudioCaptureClient* captureClient = nullptr;
    hr = client->GetService(__uuidof(IAudioCaptureClient),
                            reinterpret_cast<void**>(&captureClient));
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to get capture client");
        client->Release();
        return false;
    }

    audioClient_ = client;
    captureClient_ = captureClient;
    Logger::info(std::string(label) + ": open src " + std::to_string(srcSampleRate_) + " Hz / "
                 + std::to_string(srcChannels_) + " ch → S16 stereo 48 kHz"
                 + (srcFloat_ ? " (float)" : ""));
    return true;
}

namespace {

class ActivateCompletionHandler final : public IActivateAudioInterfaceCompletionHandler {
public:
    ActivateCompletionHandler()
        : event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)) {}
    ~ActivateCompletionHandler() {
        if (event_) {
            CloseHandle(event_);
        }
        if (client_) {
            client_->Release();
        }
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) {
            return E_POINTER;
        }
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IAgileObject)
            || riid == __uuidof(IActivateAudioInterfaceCompletionHandler)) {
            *ppv = this;
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG v = InterlockedDecrement(&ref_);
        if (v == 0) {
            delete this;
        }
        return v;
    }
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation* op) override {
        HRESULT activateHr = E_FAIL;
        IUnknown* unk = nullptr;
        if (op) {
            op->GetActivateResult(&activateHr, &unk);
        }
        hr_ = activateHr;
        if (SUCCEEDED(hr_) && unk) {
            unk->QueryInterface(__uuidof(IAudioClient), reinterpret_cast<void**>(&client_));
            unk->Release();
        }
        SetEvent(event_);
        return S_OK;
    }

    HANDLE event_ = nullptr;
    HRESULT hr_ = E_FAIL;
    IAudioClient* client_ = nullptr;

private:
    LONG ref_ = 1;
};

} // namespace

bool WasapiAudioCapture::openProcessLoopback(unsigned long pid) {
    close();
    if (pid == 0) {
        Logger::error("WASAPI process: invalid pid");
        return false;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comInit && FAILED(hr)) {
        Logger::error("WASAPI process: COM init failed");
        return false;
    }

    AUDIOCLIENT_ACTIVATION_PARAMS activationParams{};
    activationParams.ActivationType = AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK;
    activationParams.ProcessLoopbackParams.TargetProcessId = pid;
    activationParams.ProcessLoopbackParams.ProcessLoopbackMode =
        PROCESS_LOOPBACK_MODE_INCLUDE_TARGET_PROCESS_TREE;

    PROPVARIANT activateVar{};
    PropVariantInit(&activateVar);
    activateVar.vt = VT_BLOB;
    activateVar.blob.cbSize = sizeof(activationParams);
    activateVar.blob.pBlobData = reinterpret_cast<BYTE*>(&activationParams);

    auto* handler = new ActivateCompletionHandler();
    IActivateAudioInterfaceAsyncOperation* asyncOp = nullptr;
    hr = ActivateAudioInterfaceAsync(VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK,
                                     __uuidof(IAudioClient), &activateVar, handler, &asyncOp);
    if (FAILED(hr) || !asyncOp) {
        Logger::error("WASAPI process: ActivateAudioInterfaceAsync failed");
        handler->Release();
        return false;
    }

    WaitForSingleObject(handler->event_, 5000);
    asyncOp->Release();

    if (FAILED(handler->hr_) || !handler->client_) {
        Logger::error("WASAPI process: activation failed for pid " + std::to_string(pid));
        handler->Release();
        return false;
    }

    IAudioClient* client = handler->client_;
    handler->client_ = nullptr;
    handler->Release();
    return finishClientOpen(client, "WASAPI process");
}

std::vector<ProcessAudioInfo> WasapiAudioCapture::enumerateProcesses() {
    std::vector<ProcessAudioInfo> out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return out;
    }
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == 0 || pe.th32ProcessID == 4) {
                continue;
            }
            ProcessAudioInfo info;
            info.pid = pe.th32ProcessID;
            const int bytes =
                WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, nullptr, 0, nullptr, nullptr);
            std::string utf8(static_cast<size_t>(std::max(bytes - 1, 0)), '\0');
            if (bytes > 1) {
                WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, utf8.data(), bytes, nullptr,
                                    nullptr);
            }
            info.name = utf8.empty() ? ("pid " + std::to_string(info.pid)) : utf8;
            out.push_back(std::move(info));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    std::sort(out.begin(), out.end(),
              [](const ProcessAudioInfo& a, const ProcessAudioInfo& b) { return a.name < b.name; });
    return out;
}

bool WasapiAudioCapture::openEndpoint(bool capture, bool loopback, const std::string& deviceId,
                                      const char* label) {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInit = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
    if (!comInit && FAILED(hr)) {
        Logger::error(std::string(label) + ": COM init failed");
        return false;
    }

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                          __uuidof(IMMDeviceEnumerator),
                          reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to create device enumerator");
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
            Logger::warn(std::string(label) + ": device id not found, falling back to default");
            device = nullptr;
        }
    }
    if (!device) {
        const EDataFlow flow = capture ? eCapture : eRender;
        hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device);
    }
    enumerator->Release();
    if (FAILED(hr) || !device) {
        Logger::error(std::string(label) + ": no audio endpoint");
        return false;
    }

    IAudioClient* client = nullptr;
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    device->Release();
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to activate audio client");
        return false;
    }

    WAVEFORMATEX* mixFormat = nullptr;
    hr = client->GetMixFormat(&mixFormat);
    if (FAILED(hr) || !mixFormat) {
        Logger::error(std::string(label) + ": failed to get mix format");
        client->Release();
        return false;
    }

    srcSampleRate_ = static_cast<int>(mixFormat->nSamplesPerSec);
    srcChannels_ = static_cast<int>(mixFormat->nChannels);
    srcBits_ = static_cast<int>(mixFormat->wBitsPerSample);
    srcFloat_ = isFloatFormat(mixFormat);

    const DWORD streamFlags = loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    const REFERENCE_TIME bufferDuration = 10000000; // 1 second
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, bufferDuration, 0, mixFormat,
                            nullptr);
    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to initialize audio client");
        client->Release();
        return false;
    }

    IAudioCaptureClient* captureClient = nullptr;
    hr = client->GetService(__uuidof(IAudioCaptureClient),
                            reinterpret_cast<void**>(&captureClient));
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to get capture client");
        client->Release();
        return false;
    }

    audioClient_ = client;
    captureClient_ = captureClient;
    Logger::info(std::string(label) + ": open src "
                 + std::to_string(srcSampleRate_) + " Hz / " + std::to_string(srcChannels_)
                 + " ch → S16 stereo 48 kHz"
                 + (srcFloat_ ? " (float)" : ""));
    return true;
}

void WasapiAudioCapture::close() {
    stop();
    releaseResources();
}

void WasapiAudioCapture::releaseResources() {
    if (captureClient_) {
        captureClient_->Release();
        captureClient_ = nullptr;
    }
    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }
}

bool WasapiAudioCapture::start(ThreadSafeQueue<AudioFrame>* outputQueue) {
    if (running_.load() || !outputQueue || !audioClient_) {
        return false;
    }

    outputQueue_ = outputQueue;
    stopRequested_ = false;
    running_ = true;

    captureThread_ = std::make_unique<std::thread>(&WasapiAudioCapture::captureThreadFunc, this);
    return true;
}

void WasapiAudioCapture::stop() {
    stopRequested_ = true;
    if (captureThread_ && captureThread_->joinable()) {
        captureThread_->join();
    }
    captureThread_.reset();
    running_ = false;
    outputQueue_ = nullptr;
}

AudioFrame WasapiAudioCapture::convertPacket(const uint8_t* data, uint32_t numFrames) const {
    AudioFrame frame;
    frame.sampleRate = kOutRate;
    frame.channels = kOutChannels;
    frame.bytesPerSample = 2;

    if (!data || numFrames == 0 || srcChannels_ <= 0 || srcSampleRate_ <= 0) {
        return frame;
    }

    const double ratio = static_cast<double>(kOutRate) / static_cast<double>(srcSampleRate_);
    const int outFrames = std::max(1, static_cast<int>(std::lround(numFrames * ratio)));
    frame.data.resize(static_cast<size_t>(outFrames) * kOutChannels * sizeof(int16_t));
    auto* out = reinterpret_cast<int16_t*>(frame.data.data());

    for (int of = 0; of < outFrames; ++of) {
        const double srcPos = static_cast<double>(of) / ratio;
        const int i0 = std::clamp(static_cast<int>(srcPos), 0, static_cast<int>(numFrames) - 1);
        const int i1 = std::min(i0 + 1, static_cast<int>(numFrames) - 1);
        const float frac = static_cast<float>(srcPos - i0);

        for (int ch = 0; ch < kOutChannels; ++ch) {
            const float s0 =
                readSampleAsFloat(data, i0, ch, srcChannels_, srcBits_, srcFloat_);
            const float s1 =
                readSampleAsFloat(data, i1, ch, srcChannels_, srcBits_, srcFloat_);
            const float s = s0 + (s1 - s0) * frac;
            out[of * kOutChannels + ch] =
                static_cast<int16_t>(std::clamp(static_cast<int>(s * 32767.0f), -32768, 32767));
        }
    }
    return frame;
}

void WasapiAudioCapture::captureThreadFunc() {
    if (!audioClient_ || !captureClient_) {
        running_ = false;
        return;
    }

    const HRESULT hr = audioClient_->Start();
    if (FAILED(hr)) {
        Logger::error("WASAPI: failed to start capture");
        running_ = false;
        return;
    }

    while (!stopRequested_.load()) {
        UINT32 packetLength = 0;
        HRESULT pktHr = captureClient_->GetNextPacketSize(&packetLength);
        if (FAILED(pktHr)) {
            break;
        }

        while (packetLength > 0) {
            BYTE* data = nullptr;
            UINT32 numFrames = 0;
            DWORD flags = 0;

            pktHr = captureClient_->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
            if (FAILED(pktHr)) {
                break;
            }

            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT) && data && numFrames > 0) {
                AudioFrame frame = convertPacket(data, numFrames);
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                frame.timestampUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(now).count();
                if (outputQueue_ && frame.isValid()) {
                    outputQueue_->push(std::move(frame));
                }
            } else if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) && numFrames > 0) {
                // Emit silence so delay/monitoring timing stays stable.
                const double ratio =
                    static_cast<double>(kOutRate) / static_cast<double>(std::max(1, srcSampleRate_));
                const int outFrames = std::max(1, static_cast<int>(std::lround(numFrames * ratio)));
                AudioFrame frame;
                frame.sampleRate = kOutRate;
                frame.channels = kOutChannels;
                frame.bytesPerSample = 2;
                frame.data.assign(static_cast<size_t>(outFrames) * kOutChannels * sizeof(int16_t), 0);
                const auto now = std::chrono::steady_clock::now().time_since_epoch();
                frame.timestampUs =
                    std::chrono::duration_cast<std::chrono::microseconds>(now).count();
                if (outputQueue_) {
                    outputQueue_->push(std::move(frame));
                }
            }

            captureClient_->ReleaseBuffer(numFrames);
            pktHr = captureClient_->GetNextPacketSize(&packetLength);
            if (FAILED(pktHr)) {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    audioClient_->Stop();
    running_ = false;
}

} // namespace railshot
