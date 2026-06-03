#include "MediaCodecDecoder.h"
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <cstring>
#include <algorithm>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MediaCodecDecoder", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", __VA_ARGS__)

static const char* getMimeFromCodecId(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return "video/avc";
        case AV_CODEC_ID_H265: return "video/hevc";
        case AV_CODEC_ID_VP8:  return "video/x-vnd.on2.vp8";
        case AV_CODEC_ID_VP9:  return "video/x-vnd.on2.vp9";
        case AV_CODEC_ID_AV1:  return "video/av01";
        default: return nullptr;
    }
}

bool MediaCodecDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    const char* mime = getMimeFromCodecId(params->codec_id);
    if (!mime) {
        LOGE("Unsupported codec for MediaCodec: %d", params->codec_id);
        return false;
    }

    codec_ = AMediaCodec_createDecoderByType(mime);
    if (!codec_) {
        LOGE("Failed to create MediaCodec for %s", mime);
        return false;
    }

    width_ = params->width;
    height_ = params->height;

    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width_);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height_);

    if (params->extradata && params->extradata_size > 0) {
        AMediaFormat_setBuffer(format, "csd-0", params->extradata, params->extradata_size);
    }

    media_status_t status;
    if (nativeWindow_) {
        status = AMediaCodec_configure(codec_, format, nativeWindow_, nullptr, 0);
    } else {
        status = AMediaCodec_configure(codec_, format, nullptr, nullptr, 0);
    }

    AMediaFormat_delete(format);

    if (status != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed: %d", status);
        return false;
    }

    status = AMediaCodec_start(codec_);
    if (status != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed: %d", status);
        return false;
    }

    configured_ = true;
    return true;
}

bool MediaCodecDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codec_ || !configured_) return false;

    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec_, 5000);
    if (inputIndex < 0) return false;

    size_t inputSize;
    uint8_t* inputBuf = AMediaCodec_getInputBuffer(codec_, inputIndex, &inputSize);
    if (!inputBuf) return false;

    size_t copySize = std::min(static_cast<size_t>(packet->size), inputSize);
    memcpy(inputBuf, packet->data, copySize);

    uint64_t pts = packet->pts;
    AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, copySize, pts, 0);

    AMediaCodecBufferInfo info;
    ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 5000);
    if (outputIndex < 0) return false;

    bool render = (nativeWindow_ != nullptr);
    AMediaCodec_releaseOutputBuffer(codec_, outputIndex, render);

    frame->width = width_;
    frame->height = height_;
    frame->pts = static_cast<double>(info.presentationTimeUs) / 1000000.0;

    return true;
}

void MediaCodecDecoder::flush() {
    if (codec_ && configured_) {
        AMediaCodec_flush(codec_);
    }
}

void MediaCodecDecoder::destroy() {
    if (codec_) {
        if (configured_) {
            AMediaCodec_stop(codec_);
        }
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    if (nativeWindow_) {
        ANativeWindow_release(nativeWindow_);
        nativeWindow_ = nullptr;
    }
    configured_ = false;
}

AVPixelFormat MediaCodecDecoder::outputFormat() const {
    return AV_PIX_FMT_MEDIACODEC;
}

void MediaCodecDecoder::setJniEnv(JNIEnv* env) {
    jniEnv_ = env;
}

bool MediaCodecDecoder::getLastNativeTexture(NativeTexture& tex) const {
    tex.type = NativeTexture::ANDROID_SURFACE_DIRECT;
    tex.handle = nullptr;
    tex.width = width_;
    tex.height = height_;
    tex.index = 0;
    return configured_;
}

void MediaCodecDecoder::setSurface(jobject surface) {
    if (nativeWindow_) {
        ANativeWindow_release(nativeWindow_);
        nativeWindow_ = nullptr;
    }
    if (jniEnv_ && surface) {
        nativeWindow_ = ANativeWindow_fromSurface(jniEnv_, surface);
    }
}
