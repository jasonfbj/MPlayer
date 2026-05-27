#pragma once

#include "core/audio/IAudioOutput.h"
#include "core/common/NonCopyable.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <atomic>
#include <vector>

class AndroidAudioOutput : public IAudioOutput, public NonCopyable {
public:
    AndroidAudioOutput() = default;
    ~AndroidAudioOutput() override { destroy(); }

    bool init(int sampleRate, int channels, int bytesPerSample) override;
    bool start() override;
    bool stop() override;
    void setVolume(float volume) override;
    void setCallback(AudioCallback callback) override;
    void destroy() override;

private:
    static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
    void enqueueBuffer();

    SLObjectItf engineObj_ = nullptr;
    SLEngineItf engine_ = nullptr;
    SLObjectItf outputMixObj_ = nullptr;
    SLObjectItf playerObj_ = nullptr;
    SLPlayItf player_ = nullptr;
    SLAndroidSimpleBufferQueueItf bufferQueue_ = nullptr;
    SLVolumeItf volumeItf_ = nullptr;

    AudioCallback callback_;
    std::vector<uint8_t> buffer_;

    int sampleRate_ = 0;
    int channels_ = 0;
    int bytesPerSample_ = 0;
    int bufferSize_ = 0;
    std::atomic<bool> playing_{false};
};
