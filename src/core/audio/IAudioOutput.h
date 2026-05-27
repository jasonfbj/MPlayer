#pragma once

#include "core/audio/AudioFrame.h"
#include <functional>
#include <cstdint>

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    using AudioCallback = std::function<void(uint8_t* output, int size)>;

    virtual bool init(int sampleRate, int channels, int bytesPerSample) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void setVolume(float volume) = 0;
    virtual void setCallback(AudioCallback callback) = 0;
    virtual void destroy() = 0;
};
