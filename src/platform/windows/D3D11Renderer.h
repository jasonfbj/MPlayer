#pragma once

#include "core/renderer/IRenderer.h"
#include "core/common/NonCopyable.h"

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>

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

private:
    bool createDevice();
    bool createSwapChain(HWND hwnd);
    bool createRenderTargetView();
    bool createShaders();
    bool createSamplerState();
    bool createVertexBuffer();
    bool createNV12Shader();
    bool createNV12SRVs(ID3D11Texture2D* srcTexture, int index, int width, int height);

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11SamplerState> samplerState_;
    ComPtr<ID3D11Buffer> vertexBuffer_;

    ComPtr<ID3D11Texture2D> yTexture_;
    ComPtr<ID3D11Texture2D> uTexture_;
    ComPtr<ID3D11Texture2D> vTexture_;
    ComPtr<ID3D11ShaderResourceView> y_SRV_;
    ComPtr<ID3D11ShaderResourceView> u_SRV_;
    ComPtr<ID3D11ShaderResourceView> v_SRV_;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;

    // NV12 hardware decode rendering
    ComPtr<ID3D11PixelShader> nv12PixelShader_;
    ComPtr<ID3D11ShaderResourceView> nv12YSRV_;
    ComPtr<ID3D11ShaderResourceView> nv12UVSRV_;
    int hwTexWidth_ = 0;
    int hwTexHeight_ = 0;
};
