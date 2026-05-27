#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

class SoftwareDecoder : public IDecoder, public NonCopyable {
public:
    SoftwareDecoder() = default;
    ~SoftwareDecoder() override { destroy(); }

    bool init(const AVCodecParameters* params) override;
    bool decode(const AVPacket* packet, AVFrame* frame) override;
    void flush() override;
    void destroy() override;

    bool isHardware() const override { return false; }
    AVPixelFormat outputFormat() const override;

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
};
