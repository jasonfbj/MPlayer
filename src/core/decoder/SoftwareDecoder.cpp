#include "core/decoder/SoftwareDecoder.h"

bool SoftwareDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) return false;

    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) return false;

    if (avcodec_parameters_to_context(codecCtx_, params) < 0) {
        destroy();
        return false;
    }

    codecCtx_->thread_count = 0;

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        destroy();
        return false;
    }

    return true;
}

bool SoftwareDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codecCtx_) return false;

    int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return false;
    }

    ret = avcodec_receive_frame(codecCtx_, frame);
    return ret == 0;
}

void SoftwareDecoder::flush() {
    if (codecCtx_) {
        avcodec_flush_buffers(codecCtx_);
    }
}

void SoftwareDecoder::destroy() {
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    codec_ = nullptr;
}

AVPixelFormat SoftwareDecoder::outputFormat() const {
    if (!codecCtx_) return AV_PIX_FMT_NONE;
    return codecCtx_->pix_fmt;
}
