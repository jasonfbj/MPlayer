#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual bool init(const AVCodecParameters* params) = 0;
    virtual bool decode(const AVPacket* packet, AVFrame* frame) = 0;
    virtual void flush() = 0;
    virtual void destroy() = 0;

    virtual bool isHardware() const = 0;
    virtual AVPixelFormat outputFormat() const = 0;
};
