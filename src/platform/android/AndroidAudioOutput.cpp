#include "AndroidAudioOutput.h"
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MPlayer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MPlayer", __VA_ARGS__)

bool AndroidAudioOutput::init(int sampleRate, int channels, int bytesPerSample) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    bytesPerSample_ = bytesPerSample;
    bufferSize_ = sampleRate * channels * bytesPerSample / 10;
    buffer_.resize(bufferSize_);

    SLEngineOption engineOpts[] = {
        { SL_ENGINEOPTION_THREADSAFE, SL_BOOLEAN_TRUE }
    };

    SLresult result = slCreateEngine(&engineObj_, 1, engineOpts, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) { LOGE("slCreateEngine failed"); return false; }

    result = (*engineObj_)->Realize(engineObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*engineObj_)->GetInterface(engineObj_, SL_IID_ENGINE, &engine_);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*engine_)->CreateOutputMix(engine_, &outputMixObj_, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*outputMixObj_)->Realize(outputMixObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    SLDataLocator_AndroidSimpleBufferQueue locBufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };

    SLDataFormat_PCM format_pcm = {};
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = channels;
    format_pcm.samplesPerSec = sampleRate * 1000;
    format_pcm.bitsPerSample = bytesPerSample * 8;
    format_pcm.containerSize = bytesPerSample * 8;
    format_pcm.channelMask = channels == 1 ? SL_SPEAKER_FRONT_CENTER :
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audioSrc = { &locBufq, &format_pcm };

    SLDataLocator_OutputMix locOutmix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObj_ };
    SLDataSink audioSnk = { &locOutmix, nullptr };

    SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
    SLboolean req[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    result = (*engine_)->CreateAudioPlayer(engine_, &playerObj_, &audioSrc, &audioSnk,
        2, ids, req);
    if (result != SL_RESULT_SUCCESS) { LOGE("CreateAudioPlayer failed"); return false; }

    result = (*playerObj_)->Realize(playerObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*playerObj_)->GetInterface(playerObj_, SL_IID_PLAY, &player_);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*playerObj_)->GetInterface(playerObj_, SL_IID_BUFFERQUEUE, &bufferQueue_);
    if (result != SL_RESULT_SUCCESS) return false;

    (*playerObj_)->GetInterface(playerObj_, SL_IID_VOLUME, &volumeItf_);

    result = (*bufferQueue_)->RegisterCallback(bufferQueue_, bqPlayerCallback, this);
    if (result != SL_RESULT_SUCCESS) return false;

    return true;
}

bool AndroidAudioOutput::start() {
    if (!player_) return false;
    playing_ = true;
    (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING);
    enqueueBuffer();
    return true;
}

bool AndroidAudioOutput::stop() {
    if (!player_) return false;
    playing_ = false;
    (*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED);
    return true;
}

void AndroidAudioOutput::setVolume(float volume) {
    if (volumeItf_) {
        SLmillibel mb = static_cast<SLmillibel>((1.0f - volume) * SL_MILLIBEL_MIN);
        (*volumeItf_)->SetVolumeLevel(volumeItf_, mb);
    }
}

void AndroidAudioOutput::setCallback(AudioCallback callback) {
    callback_ = std::move(callback);
}

void AndroidAudioOutput::destroy() {
    stop();
    if (playerObj_) { (*playerObj_)->Destroy(playerObj_); playerObj_ = nullptr; }
    if (outputMixObj_) { (*outputMixObj_)->Destroy(outputMixObj_); outputMixObj_ = nullptr; }
    if (engineObj_) { (*engineObj_)->Destroy(engineObj_); engineObj_ = nullptr; }
}

void AndroidAudioOutput::bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* self = static_cast<AndroidAudioOutput*>(context);
    self->enqueueBuffer();
}

void AndroidAudioOutput::enqueueBuffer() {
    if (!playing_ || !callback_) return;

    callback_(buffer_.data(), bufferSize_);
    (*bufferQueue_)->Enqueue(bufferQueue_, buffer_.data(), bufferSize_);
}
