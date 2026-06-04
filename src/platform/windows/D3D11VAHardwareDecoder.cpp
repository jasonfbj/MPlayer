#include "D3D11VAHardwareDecoder.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

void D3D11VAHardwareDecoder::setSharedDevice(ID3D11Device* device) {
    sharedDevice_ = device;
    if (device) {
        device->GetImmediateContext(&sharedContext_);
    }
}

bool D3D11VAHardwareDecoder::getLastNativeTexture(NativeTexture& tex) const {
    if (!lastTexture_) return false;

    tex.type = NativeTexture::D3D11_TEXTURE;
    tex.handle = lastTexture_.Get();
    tex.index = lastIndex_;
    tex.width = lastWidth_;
    tex.height = lastHeight_;
    return true;
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

    if (sharedDevice_) {
        // Use shared device from renderer
        hwDeviceCtx_ = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!hwDeviceCtx_) {
            destroy();
            return false;
        }

        auto* hwCtx = reinterpret_cast<AVHWDeviceContext*>(hwDeviceCtx_->data);
        auto* d3d11Ctx = reinterpret_cast<AVD3D11VADeviceContext*>(hwCtx->hwctx);

        // AddRef before handing to FFmpeg — FFmpeg will Release() on cleanup
        sharedDevice_->AddRef();
        sharedContext_->AddRef();
        d3d11Ctx->device = sharedDevice_.Get();
        d3d11Ctx->device_context = sharedContext_.Get();
        d3d11Ctx->lock_ctx = contextMutex_;
        d3d11Ctx->lock = [](void* lock_ctx) {
            static_cast<std::mutex*>(lock_ctx)->lock();
        };
        d3d11Ctx->unlock = [](void* lock_ctx) {
            static_cast<std::mutex*>(lock_ctx)->unlock();
        };

        if (av_hwdevice_ctx_init(hwDeviceCtx_) < 0) {
            destroy();
            return false;
        }
    } else {
        // Create our own device
        int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D11VA,
            nullptr, nullptr, 0);
        if (ret < 0) {
            destroy();
            return false;
        }
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

    lastWidth_ = params->width;
    lastHeight_ = params->height;
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
            lastTexture_ = desc->texture;
            lastIndex_ = desc->index;
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
    lastTexture_.Reset();
    sharedDevice_.Reset();
    sharedContext_.Reset();
    lastIndex_ = 0;
    lastWidth_ = 0;
    lastHeight_ = 0;
    codec_ = nullptr;
    initialized_ = false;
}

AVPixelFormat D3D11VAHardwareDecoder::outputFormat() const {
    return AV_PIX_FMT_D3D11;
}
