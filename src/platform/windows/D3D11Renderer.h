#pragma once

#include "core/renderer/IRenderer.h"
#include "core/common/NonCopyable.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <mutex>
#include <functional>

using Microsoft::WRL::ComPtr;

class D3D11Renderer : public IRenderer, public NonCopyable {
public:
    D3D11Renderer() = default;
    ~D3D11Renderer() override { destroy(); }

    bool init(void* nativeWindow) override;
    bool renderFrame(const VideoFrame& frame) override;
    bool renderTexture(const NativeTexture& texture) override;
    void resize(int width, int height) override;
    void destroy() override;

    void* getNativeDevice() const override { return device_.Get(); }
    void* getNativeDeviceContext() const override { return context_.Get(); }

    // Mutex for synchronizing D3D11 immediate context access with hardware decoder
    std::mutex& contextMutex() { return contextMutex_; }

    // Check and handle device removed/reset after Present
    bool handleDeviceLost();

    // Set callback to invoke when D3D11 device is recreated after device lost.
    // The callback receives the new ID3D11Device* so consumers (e.g. hardware decoder) can update.
    using DeviceRestoredCallback = std::function<void(void* newDevice)>;
    void setDeviceRestoredCallback(DeviceRestoredCallback cb) { deviceRestoredCb_ = std::move(cb); }

private:
    bool createDevice();
    bool createSwapChain(HWND hwnd);
    bool createRenderTargetView();
    bool createShaders();
    bool createSamplerState();
    bool createVertexBuffer();
    bool createNV12Shader();
    bool createNV12SRVs(ID3D11Texture2D* srcTexture, int index, int width, int height);
    void setupDrawState();
    void destroyResources();  // Internal: release GPU resources without locking

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    ComPtr<ID3D11SamplerState> samplerState_;
    ComPtr<ID3D11Buffer> vertexBuffer_;

    ComPtr<ID3D11Texture2D> yTexture_;
    ComPtr<ID3D11Texture2D> uTexture_;
    ComPtr<ID3D11Texture2D> vTexture_;
    ComPtr<ID3D11ShaderResourceView> y_SRV_;
    ComPtr<ID3D11ShaderResourceView> u_SRV_;
    ComPtr<ID3D11ShaderResourceView> v_SRV_;
    int swTexWidth_ = 0;
    int swTexHeight_ = 0;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    HWND hwnd_ = nullptr;  // Saved for device-lost reinitialization

    // NV12 hardware decode rendering
    ComPtr<ID3D11PixelShader> nv12PixelShader_;
    ComPtr<ID3D11ShaderResourceView> nv12YSRV_;
    ComPtr<ID3D11ShaderResourceView> nv12UVSRV_;
    ComPtr<ID3D11Texture2D> hwCopyTexture_;  // Shader-visible copy of decoded texture
    int hwTexWidth_ = 0;
    int hwTexHeight_ = 0;

    // Mutex shared with D3D11VA hardware decoder for context synchronization
    std::mutex contextMutex_;

    // Callback invoked after device is recreated in handleDeviceLost()
    DeviceRestoredCallback deviceRestoredCb_;
};
