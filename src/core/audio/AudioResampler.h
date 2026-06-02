#pragma once

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#include <cstdint>
#include <vector>

class AudioResampler {
public:
    AudioResampler() = default;
    ~AudioResampler();

    bool init(int sampleRate, int channels, AVSampleFormat format);
    bool setSpeed(float speed);
    float getSpeed() const { return speed_; }
    bool process(const AVFrame* input, std::vector<uint8_t>& output);
    void destroy();

private:
    AVFilterGraph* filterGraph_ = nullptr;
    AVFilterContext* srcCtx_ = nullptr;
    AVFilterContext* atempoCtx_ = nullptr;
    AVFilterContext* sinkCtx_ = nullptr;

    int sampleRate_ = 0;
    int channels_ = 0;
    AVSampleFormat format_ = AV_SAMPLE_FMT_NONE;
    float speed_ = 1.0f;
    bool initialized_ = false;
};
