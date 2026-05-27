#include "D3D11Renderer.h"
#include <d3dcompiler.h>
#include <cstring>

bool D3D11Renderer::init(void* nativeWindow) {
    HWND hwnd = static_cast<HWND>(nativeWindow);
    if (!hwnd) return false;

    if (!createDevice()) return false;
    if (!createSwapChain(hwnd)) return false;
    if (!createRenderTargetView()) return false;
    if (!createShaders()) return false;
    if (!createSamplerState()) return false;
    if (!createVertexBuffer()) return false;

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

bool D3D11Renderer::renderFrame(const VideoFrame& frame) {
    if (!initialized_ || frame.format == VideoFrame::NativeTexture) return false;

    auto createOrUpdateTexture = [&](ComPtr<ID3D11Texture2D>& tex,
                                     ComPtr<ID3D11ShaderResourceView>& srv,
                                     const uint8_t* data, int width, int height) {
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

    UINT stride = sizeof(float) * 4;
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context_->Draw(4, 0);

    swapChain_->Present(1, 0);
    return true;
}

bool D3D11Renderer::renderTexture(void* nativeTexture, int w, int h) {
    // 硬解纹理渲染 - 后续实现
    return false;
}

void D3D11Renderer::resize(int width, int height) {
    if (width == width_ && height == height_) return;

    renderTargetView_.Reset();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createRenderTargetView();

    width_ = width;
    height_ = height;
}

void D3D11Renderer::destroy() {
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
