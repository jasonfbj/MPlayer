#include "core/audio/AudioResampler.h"
#include <cstdio>

AudioResampler::~AudioResampler() {
    destroy();
}

bool AudioResampler::init(int sampleRate, int channels, AVSampleFormat format) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    format_ = format;

    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) return false;

    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    if (!abuffer) { destroy(); return false; }

    char args[512];
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=1/%d",
        sampleRate,
        av_get_sample_fmt_name(format),
        channels,
        sampleRate);

    int ret = avfilter_graph_create_filter(&srcCtx_, abuffer, "src",
        args, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    const AVFilter* atempo = avfilter_get_by_name("atempo");
    if (!atempo) { destroy(); return false; }

    char tempoArgs[32];
    snprintf(tempoArgs, sizeof(tempoArgs), "tempo=%f", 1.0);

    ret = avfilter_graph_create_filter(&atempoCtx_, atempo, "atempo",
        tempoArgs, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    const AVFilter* sink = avfilter_get_by_name("abuffersink");
    if (!sink) { destroy(); return false; }

    ret = avfilter_graph_create_filter(&sinkCtx_, sink, "sink",
        nullptr, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    // Force S16 interleaved output so data[0] is always contiguous
    const enum AVSampleFormat out_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
    av_opt_set_int_list(sinkCtx_, "sample_fmts", out_fmts, AV_SAMPLE_FMT_NONE,
                        AV_OPT_SEARCH_CHILDREN);

    ret = avfilter_link(srcCtx_, 0, atempoCtx_, 0);
    if (ret < 0) { destroy(); return false; }

    ret = avfilter_link(atempoCtx_, 0, sinkCtx_, 0);
    if (ret < 0) { destroy(); return false; }

    ret = avfilter_graph_config(filterGraph_, nullptr);
    if (ret < 0) { destroy(); return false; }

    initialized_ = true;
    return true;
}

bool AudioResampler::setSpeed(float speed) {
    if (!atempoCtx_ || speed < 0.5f || speed > 100.0f) return false;
    speed_ = speed;

    char str[32];
    snprintf(str, sizeof(str), "%f", speed);
    return av_opt_set(atempoCtx_->priv, "tempo", str, AV_OPT_SEARCH_CHILDREN) >= 0;
}

bool AudioResampler::process(const AVFrame* input, std::vector<uint8_t>& output) {
    if (!initialized_ || !srcCtx_ || !sinkCtx_) return false;

    output.clear();

    int ret = av_buffersrc_add_frame_flags(srcCtx_,
        const_cast<AVFrame*>(input), AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) return false;

    // Drain all available frames from the filter graph
    while (true) {
        AVFrame* filtFrame = av_frame_alloc();
        ret = av_buffersink_get_frame(sinkCtx_, filtFrame);
        if (ret < 0) {
            av_frame_free(&filtFrame);
            break;  // No more frames available (AVERROR(EAGAIN) or EOF)
        }

        int dataSize = av_samples_get_buffer_size(nullptr,
            filtFrame->ch_layout.nb_channels,
            filtFrame->nb_samples,
            static_cast<AVSampleFormat>(filtFrame->format), 1);

        if (dataSize > 0 && filtFrame->data[0]) {
            size_t oldSize = output.size();
            output.resize(oldSize + dataSize);
            memcpy(output.data() + oldSize, filtFrame->data[0], dataSize);
        }

        av_frame_free(&filtFrame);
    }

    return !output.empty();
}

void AudioResampler::destroy() {
    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
    }
    srcCtx_ = nullptr;
    atempoCtx_ = nullptr;
    sinkCtx_ = nullptr;
    initialized_ = false;
}
