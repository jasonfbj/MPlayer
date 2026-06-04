#include "WinAudioOutput.h"
#include <algorithm>

bool WinAudioOutput::init(int sampleRate, int channels, int bytesPerSample) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    bytesPerSample_ = bytesPerSample;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    enumerator->Release();
    if (FAILED(hr)) return false;

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(&audioClient_));
    if (FAILED(hr)) return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = bytesPerSample * 8;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    REFERENCE_TIME bufferDuration = 500000;

    hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
        0, bufferDuration, 0, &wfx, nullptr);
    if (FAILED(hr)) return false;

    hr = audioClient_->GetBufferSize(&bufferFrameCount_);
    if (FAILED(hr)) return false;

    hr = audioClient_->GetService(__uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&renderClient_));
    if (FAILED(hr)) return false;

    audioClient_->GetService(__uuidof(IAudioStreamVolume),
        reinterpret_cast<void**>(&streamVolume_));

    initialized_ = true;
    return true;
}

bool WinAudioOutput::start() {
    if (running_) return true;
    if (!initialized_) return true;  // Not initialized (e.g. no audio stream) — skip

    running_ = true;
    thread_ = std::thread(&WinAudioOutput::audioThread, this);

    if (audioClient_) {
        audioClient_->Start();
    }
    return true;
}

bool WinAudioOutput::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (audioClient_) {
        audioClient_->Stop();
    }
    return true;
}

void WinAudioOutput::setVolume(float volume) {
    if (streamVolume_) {
        uint32_t channelCount = 0;
        streamVolume_->GetChannelCount(&channelCount);
        std::vector<float> volumes(channelCount, volume);
        streamVolume_->SetAllVolumes(channelCount, volumes.data());
    }
}

void WinAudioOutput::setCallback(AudioCallback callback) {
    callback_ = std::move(callback);
}

void WinAudioOutput::destroy() {
    stop();

    if (streamVolume_) { streamVolume_->Release(); streamVolume_ = nullptr; }
    if (renderClient_) { renderClient_->Release(); renderClient_ = nullptr; }
    if (audioClient_) { audioClient_->Release(); audioClient_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void WinAudioOutput::audioThread() {
    while (running_) {
        UINT32 padding = 0;
        audioClient_->GetCurrentPadding(&padding);
        UINT32 availableFrames = bufferFrameCount_ - padding;

        if (availableFrames == 0) {
            Sleep(1);
            continue;
        }

        BYTE* data = nullptr;
        HRESULT hr = renderClient_->GetBuffer(availableFrames, &data);
        if (SUCCEEDED(hr)) {
            if (callback_) {
                UINT32 size = availableFrames * channels_ * bytesPerSample_;
                callback_(data, size);
            } else {
                memset(data, 0, availableFrames * channels_ * bytesPerSample_);
            }
            renderClient_->ReleaseBuffer(availableFrames, 0);
        }
    }
}
