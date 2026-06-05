#include "D3D11Renderer.h"
#include <d3dcompiler.h>
#include <cstring>

bool D3D11Renderer::init(void* nativeWindow) {
    HWND hwnd = static_cast<HWND>(nativeWindow);
    if (!hwnd) return false;

    hwnd_ = hwnd;

    if (!createDevice()) return false;
    if (!createSwapChain(hwnd)) return false;
    if (!createRenderTargetView()) return false;
    if (!createShaders()) return false;
    if (!createSamplerState()) return false;
    if (!createVertexBuffer()) return false;
    if (!createNV12Shader()) return false;

    initialized_ = true;
    return true;
}

bool D3D11Renderer::createDevice() {
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL selectedLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, 1, D3D11_SDK_VERSION,
        &device_, &selectedLevel, &context_
    );

    return SUCCEEDED(hr);
}

bool D3D11Renderer::createSwapChain(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferCount = 2;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.Windowed = TRUE;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

    ComPtr<IDXGIDevice> dxgiDevice;
    device_->QueryInterface(__uuidof(IDXGIDevice), &dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIFactory> factory;
    adapter->GetParent(__uuidof(IDXGIFactory), &factory);

    return SUCCEEDED(factory->CreateSwapChain(device_.Get(), &desc, &swapChain_));
}

bool D3D11Renderer::createRenderTargetView() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr)) return false;

    return SUCCEEDED(device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_));
}

bool D3D11Renderer::createShaders() {
    const char* vsSource = R"(
        struct VS_INPUT { float2 pos : POSITION; float2 uv : TEXCOORD; };
        struct VS_OUTPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
        VS_OUTPUT VS(VS_INPUT input) {
            VS_OUTPUT output;
            output.pos = float4(input.pos, 0.0, 1.0);
            output.uv = input.uv;
            return output;
        }
    )";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "VS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) return false;

    hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &vertexShader_);
    if (FAILED(hr)) return false;

    // Create input layout matching VS_INPUT: {float2 pos, float2 uv}
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device_->CreateInputLayout(layout, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout_);
    if (FAILED(hr)) return false;

    const char* psSource = R"(
        Texture2D<float> texY : register(t0);
        Texture2D<float> texU : register(t1);
        Texture2D<float> texV : register(t2);
        SamplerState sampler0 : register(s0);
        struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
        float4 PS(PS_INPUT input) : SV_TARGET {
            float y = texY.Sample(sampler0, input.uv);
            float u = texU.Sample(sampler0, input.uv) - 0.5f;
            float v = texV.Sample(sampler0, input.uv) - 0.5f;
            float r = y + 1.402 * v;
            float g = y - 0.344136 * u - 0.714136 * v;
            float b = y + 1.772 * u;
            return float4(r, g, b, 1.0);
        }
    )";

    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) return false;

    return SUCCEEDED(device_->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &pixelShader_));
}

bool D3D11Renderer::createSamplerState() {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    return SUCCEEDED(device_->CreateSamplerState(&desc, &samplerState_));
}

bool D3D11Renderer::createVertexBuffer() {
    struct Vertex { float x, y, u, v; };
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f },
    };

    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = sizeof(vertices);
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA data = {};
    data.pSysMem = vertices;

    return SUCCEEDED(device_->CreateBuffer(&desc, &data, &vertexBuffer_));
}

void D3D11Renderer::setupDrawState() {
    // Set viewport to match render target
    D3D11_VIEWPORT vp = {};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(width_);
    vp.Height = static_cast<float>(height_);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &vp);

    // Bind input layout and vertex buffer
    context_->IASetInputLayout(inputLayout_.Get());
    UINT stride = sizeof(float) * 4;
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

bool D3D11Renderer::renderFrame(const VideoFrame& frame) {
    if (!initialized_ || frame.format == VideoFrame::NativeTexture) return false;

    // Validate frame data before rendering
    if (frame.width <= 0 || frame.height <= 0) return false;
    if (frame.format != VideoFrame::YUV420P) return false;
    if (frame.data[0].empty() || frame.data[1].empty() || frame.data[2].empty())
        return false;

    std::unique_lock<std::mutex> lock(contextMutex_);

    auto createOrUpdateTexture = [&](ComPtr<ID3D11Texture2D>& tex,
                                     ComPtr<ID3D11ShaderResourceView>& srv,
                                     const uint8_t* data, int width, int height) {
        // Recreate texture if dimensions changed
        if (tex && (swTexWidth_ != frame.width || swTexHeight_ != frame.height)) {
            tex.Reset();
            srv.Reset();
        }
        if (!tex) {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            if (FAILED(device_->CreateTexture2D(&desc, nullptr, &tex))) return false;
            if (FAILED(device_->CreateShaderResourceView(tex.Get(), nullptr, &srv))) return false;
            swTexWidth_ = frame.width;
            swTexHeight_ = frame.height;
        }

        D3D11_MAPPED_SUBRESOURCE mapped;
        if (FAILED(context_->Map(tex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) return false;

        for (int y = 0; y < height; y++) {
            memcpy(static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch,
                   data + y * width, width);
        }
        context_->Unmap(tex.Get(), 0);
        return true;
    };

    int halfW = frame.width / 2;
    int halfH = frame.height / 2;

    if (!createOrUpdateTexture(yTexture_, y_SRV_, frame.data[0].data(), frame.width, frame.height)) return false;
    if (!createOrUpdateTexture(uTexture_, u_SRV_, frame.data[1].data(), halfW, halfH)) return false;
    if (!createOrUpdateTexture(vTexture_, v_SRV_, frame.data[2].data(), halfW, halfH)) return false;

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    ID3D11ShaderResourceView* srvs[] = { y_SRV_.Get(), u_SRV_.Get(), v_SRV_.Get() };
    context_->PSSetShaderResources(0, 3, srvs);
    context_->PSSetSamplers(0, 1, samplerState_.GetAddressOf());

    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);

    setupDrawState();
    context_->Draw(4, 0);

    HRESULT hr = swapChain_->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        lock.unlock();
        handleDeviceLost();
        return false;
    }
    return true;
}

bool D3D11Renderer::renderTexture(const NativeTexture& texture) {
    if (!initialized_) return false;
    if (texture.type != NativeTexture::D3D11_TEXTURE || !texture.handle) return false;

    std::unique_lock<std::mutex> lock(contextMutex_);

    auto* d3dTexture = static_cast<ID3D11Texture2D*>(texture.handle);
    if (!createNV12SRVs(d3dTexture, texture.index, texture.width, texture.height)) return false;

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    ID3D11ShaderResourceView* srvs[] = { nv12YSRV_.Get(), nv12UVSRV_.Get() };
    context_->PSSetShaderResources(0, 2, srvs);
    context_->PSSetSamplers(0, 1, samplerState_.GetAddressOf());

    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(nv12PixelShader_.Get(), nullptr, 0);

    setupDrawState();
    context_->Draw(4, 0);

    HRESULT hr = swapChain_->Present(1, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        lock.unlock();
        handleDeviceLost();
        return false;
    }
    return true;
}

bool D3D11Renderer::handleDeviceLost() {
    // Caller has already released contextMutex_ before calling us.
    // destroyResources() does NOT lock — it just releases GPU resources.
    destroyResources();
    if (!hwnd_) return false;
    if (!init(hwnd_)) return false;

    // Notify consumers (e.g. hardware decoder) that the device was recreated
    if (deviceRestoredCb_) {
        deviceRestoredCb_(device_.Get());
    }
    return true;
}

bool D3D11Renderer::createNV12Shader() {
    const char* psSource = R"(
        Texture2D<float> texY : register(t0);
        Texture2D<float2> texUV : register(t1);
        SamplerState sampler0 : register(s0);
        struct PS_INPUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD; };
        float4 PS(PS_INPUT input) : SV_TARGET {
            float y = texY.Sample(sampler0, input.uv);
            float2 uv = texUV.Sample(sampler0, input.uv);
            float u = uv.x - 0.5f;
            float v = uv.y - 0.5f;
            float r = y + 1.402f * v;
            float g = y - 0.344136f * u - 0.714136f * v;
            float b = y + 1.772f * u;
            return float4(r, g, b, 1.0f);
        }
    )";

    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) return false;

    return SUCCEEDED(device_->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &nv12PixelShader_));
}

bool D3D11Renderer::createNV12SRVs(ID3D11Texture2D* srcTexture, int index, int width, int height) {
    if (!srcTexture) return false;

    // Get the source texture's actual dimensions (may have alignment padding)
    D3D11_TEXTURE2D_DESC srcDesc;
    srcTexture->GetDesc(&srcDesc);

    // Recreate the shader-visible copy texture if source dimensions changed
    if (!hwCopyTexture_ || srcDesc.Width != hwTexWidth_ || srcDesc.Height != hwTexHeight_) {
        hwCopyTexture_.Reset();
        nv12YSRV_.Reset();
        nv12UVSRV_.Reset();

        D3D11_TEXTURE2D_DESC copyDesc = {};
        copyDesc.Width = srcDesc.Width;
        copyDesc.Height = srcDesc.Height;
        copyDesc.MipLevels = 1;
        copyDesc.ArraySize = 1;
        copyDesc.Format = srcDesc.Format;
        copyDesc.SampleDesc.Count = 1;
        copyDesc.Usage = D3D11_USAGE_DEFAULT;
        copyDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        if (FAILED(device_->CreateTexture2D(&copyDesc, nullptr, &hwCopyTexture_)))
            return false;

        hwTexWidth_ = srcDesc.Width;
        hwTexHeight_ = srcDesc.Height;

        // Create Y plane SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC yDesc = {};
        yDesc.Format = DXGI_FORMAT_R8_UNORM;
        yDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        yDesc.Texture2D.MipLevels = 1;
        yDesc.Texture2D.MostDetailedMip = 0;

        if (FAILED(device_->CreateShaderResourceView(hwCopyTexture_.Get(), &yDesc, &nv12YSRV_)))
            return false;

        // Create UV plane SRV
        D3D11_SHADER_RESOURCE_VIEW_DESC uvDesc = {};
        uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
        uvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        uvDesc.Texture2D.MipLevels = 1;
        uvDesc.Texture2D.MostDetailedMip = 0;

        if (FAILED(device_->CreateShaderResourceView(hwCopyTexture_.Get(), &uvDesc, &nv12UVSRV_))) {
            nv12YSRV_.Reset();
            return false;
        }
    }

    // Copy decoded frame (srcTexture array slice [index]) to our shader-visible texture
    context_->CopySubresourceRegion(hwCopyTexture_.Get(), 0, 0, 0, 0, srcTexture, index, nullptr);

    return true;
}

void D3D11Renderer::resize(int width, int height) {
    if (!initialized_) return;
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_) return;

    std::lock_guard<std::mutex> lock(contextMutex_);

    renderTargetView_.Reset();
    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        // Device lost — update dimensions so handleDeviceLost uses correct size
        width_ = width;
        height_ = height;
        return;
    }
    if (SUCCEEDED(hr)) {
        createRenderTargetView();
        width_ = width;
        height_ = height;
    }
}

void D3D11Renderer::destroy() {
    std::lock_guard<std::mutex> lock(contextMutex_);
    destroyResources();
}

void D3D11Renderer::destroyResources() {
    nv12UVSRV_.Reset();
    nv12YSRV_.Reset();
    hwCopyTexture_.Reset();
    nv12PixelShader_.Reset();
    inputLayout_.Reset();
    hwTexWidth_ = 0;
    hwTexHeight_ = 0;
    swTexWidth_ = 0;
    swTexHeight_ = 0;
    y_SRV_.Reset(); u_SRV_.Reset(); v_SRV_.Reset();
    yTexture_.Reset(); uTexture_.Reset(); vTexture_.Reset();
    vertexBuffer_.Reset();
    samplerState_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    renderTargetView_.Reset();
    swapChain_.Reset();
    context_.Reset();
    device_.Reset();
    initialized_ = false;
}
