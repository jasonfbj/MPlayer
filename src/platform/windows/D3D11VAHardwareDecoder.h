#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"
#include "core/common/VideoFrame.h"

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

    // Shared device from renderer — gets context internally via GetImmediateContext
    void setSharedDevice(ID3D11Device* device);
    bool getLastNativeTexture(NativeTexture& tex) const;

    ID3D11Texture2D* getLastDecodedTexture() const { return lastTexture_.Get(); }

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> decodedTexture_;
    bool initialized_ = false;

    // Shared device support
    ID3D11Device* sharedDevice_ = nullptr;
    ID3D11DeviceContext* sharedContext_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> lastTexture_;
    int lastIndex_ = 0;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
};
