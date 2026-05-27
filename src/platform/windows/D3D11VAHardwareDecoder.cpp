#include "D3D11VAHardwareDecoder.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

bool D3D11VAHardwareDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) return false;

    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) return false;

    if (avcodec_parameters_to_context(codecCtx_, params) < 0) {
        destroy();
        return false;
    }

    int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D11VA,
        nullptr, nullptr, 0);
    if (ret < 0) {
        destroy();
        return false;
    }

    codecCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
    codecCtx_->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
        for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_D3D11) return *p;
        }
        return AV_PIX_FMT_NONE;
    };

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        destroy();
        return false;
    }

    initialized_ = true;
    return true;
}

bool D3D11VAHardwareDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codecCtx_) return false;

    int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0) return false;

    ret = avcodec_receive_frame(codecCtx_, frame);
    if (ret < 0) return false;

    if (frame->format == AV_PIX_FMT_D3D11) {
        auto* desc = reinterpret_cast<AVD3D11FrameDescriptor*>(frame->data[0]);
        if (desc && desc->texture) {
            decodedTexture_ = desc->texture;
        }
    }

    return true;
}

void D3D11VAHardwareDecoder::flush() {
    if (codecCtx_) avcodec_flush_buffers(codecCtx_);
}

void D3D11VAHardwareDecoder::destroy() {
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (hwDeviceCtx_) {
        av_buffer_unref(&hwDeviceCtx_);
        hwDeviceCtx_ = nullptr;
    }
    decodedTexture_.Reset();
    codec_ = nullptr;
    initialized_ = false;
}

AVPixelFormat D3D11VAHardwareDecoder::outputFormat() const {
    return AV_PIX_FMT_D3D11;
}
