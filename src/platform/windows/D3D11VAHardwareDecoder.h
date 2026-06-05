#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"
#include "core/common/VideoFrame.h"

#include <d3d11.h>
#include <wrl/client.h>
#include <mutex>

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
    void setContextMutex(std::mutex* mtx) { contextMutex_ = mtx; }

    // Called when the D3D11 device is recreated after device lost.
    // Tears down the old FFmpeg hw device context and reinitializes with the new device.
    void onDeviceRestored(ID3D11Device* newDevice);
    bool getLastNativeTexture(NativeTexture& tex) const;

    ID3D11Texture2D* getLastDecodedTexture() const { return lastTexture_.Get(); }

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    bool initialized_ = false;

    // Shared device support (ComPtr for safe reference counting)
    Microsoft::WRL::ComPtr<ID3D11Device> sharedDevice_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> sharedContext_;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> lastTexture_;
    int lastIndex_ = 0;
    int lastWidth_ = 0;
    int lastHeight_ = 0;

    // Pointer to renderer's mutex — shared for D3D11 context synchronization
    std::mutex* contextMutex_ = nullptr;
};
