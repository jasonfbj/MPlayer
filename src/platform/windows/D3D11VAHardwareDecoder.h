#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

#include <d3d11.h>
#include <wrl/client.h>

class D3D11VAHardwareDecoder : public IDecoder, public NonCopyable {
public:
    D3D11VAHardwareDecoder() = default;
    ~D3D11VAHardwareDecoder() override { destroy(); }

    bool init(const AVCodecParameters* params) override;
    bool decode(const AVPacket* packet, AVFrame* frame) override;
    void flush() override;
    void destroy() override;

    bool isHardware() const override { return true; }
    AVPixelFormat outputFormat() const override;

    ID3D11Texture2D* getLastDecodedTexture() const { return decodedTexture_.Get(); }

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> decodedTexture_;
    bool initialized_ = false;
};
