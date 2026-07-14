#include "capture/WasapiAudioCapture.h"

#include "core/Logger.h"

#include <audioclient.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <chrono>
#include <cstring>

namespace railshot {

WasapiAudioCapture::WasapiAudioCapture() = default;

WasapiAudioCapture::~WasapiAudioCapture() {
    stop();
    close();
}

namespace {

bool openWasapiEndpoint(EDataFlow flow, DWORD streamFlags, IAudioClient** outClient,
                        IAudioCaptureClient** outCapture, int& sampleRate, int& channels,
                        int& bytesPerSample, const char* label) {
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
    hr = enumerator->GetDefaultAudioEndpoint(flow, eConsole, &device);
    enumerator->Release();
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": no default endpoint");
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

    sampleRate = static_cast<int>(mixFormat->nSamplesPerSec);
    channels = static_cast<int>(mixFormat->nChannels);
    bytesPerSample = static_cast<int>(mixFormat->wBitsPerSample) / 8;

    const REFERENCE_TIME bufferDuration = 10000000; // 1 second
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, bufferDuration, 0, mixFormat,
                            nullptr);
    CoTaskMemFree(mixFormat);

    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to initialize audio client");
        client->Release();
        return false;
    }

    IAudioCaptureClient* capture = nullptr;
    hr = client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture));
    if (FAILED(hr)) {
        Logger::error(std::string(label) + ": failed to get capture client");
        client->Release();
        return false;
    }

    *outClient = client;
    *outCapture = capture;
    return true;
}

} // namespace

bool WasapiAudioCapture::openDefaultMicrophone() {
    close();
    if (!openWasapiEndpoint(eCapture, 0, &audioClient_, &captureClient_, sampleRate_, channels_,
                            bytesPerSample_, "WASAPI mic")) {
        releaseResources();
        return false;
    }
    Logger::info("WASAPI: opened default microphone at " + std::to_string(sampleRate_) + " Hz, "
                 + std::to_string(channels_) + " channels");
    return true;
}

bool WasapiAudioCapture::openDefaultDesktopLoopback() {
    close();
    if (!openWasapiEndpoint(eRender, AUDCLNT_STREAMFLAGS_LOOPBACK, &audioClient_, &captureClient_,
                            sampleRate_, channels_, bytesPerSample_, "WASAPI desktop")) {
        releaseResources();
        return false;
    }
    Logger::info("WASAPI: opened desktop loopback at " + std::to_string(sampleRate_) + " Hz, "
                 + std::to_string(channels_) + " channels");
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
                AudioFrame frame;
                frame.sampleRate = sampleRate_;
                frame.channels = channels_;
                frame.bytesPerSample = bytesPerSample_;

                const size_t byteCount = static_cast<size_t>(numFrames) *
                                         static_cast<size_t>(channels_) *
                                         static_cast<size_t>(bytesPerSample_);
                frame.data.resize(byteCount);
                std::memcpy(frame.data.data(), data, byteCount);

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
