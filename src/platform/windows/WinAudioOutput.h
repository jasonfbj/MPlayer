#pragma once

#include "core/audio/IAudioOutput.h"
#include "core/common/NonCopyable.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>

class WinAudioOutput : public IAudioOutput, public NonCopyable {
public:
    WinAudioOutput() = default;
    ~WinAudioOutput() override { destroy(); }

    bool init(int sampleRate, int channels, int bytesPerSample) override;
    bool start() override;
    bool stop() override;
    void setVolume(float volume) override;
    void setCallback(AudioCallback callback) override;
    void destroy() override;

private:
    void audioThread();

    IMMDevice* device_ = nullptr;
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    IAudioStreamVolume* streamVolume_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
    AudioCallback callback_;

    int sampleRate_ = 0;
    int channels_ = 0;
    int bytesPerSample_ = 0;
    UINT32 bufferFrameCount_ = 0;
};
