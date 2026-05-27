#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>

class MediaCodecDecoder : public IDecoder, public NonCopyable {
public:
    MediaCodecDecoder() = default;
    ~MediaCodecDecoder() override { destroy(); }

    bool init(const AVCodecParameters* params) override;
    bool decode(const AVPacket* packet, AVFrame* frame) override;
    void flush() override;
    void destroy() override;

    bool isHardware() const override { return true; }
    AVPixelFormat outputFormat() const override;

    void setSurface(jobject surface);

private:
    AMediaCodec* codec_ = nullptr;
    jobject surface_ = nullptr;
    bool configured_ = false;
    int width_ = 0;
    int height_ = 0;
};
