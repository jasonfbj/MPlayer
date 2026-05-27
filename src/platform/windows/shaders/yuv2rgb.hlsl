// MPlayer - D3D11 YUV→RGB 着色器
// 此文件为参考，实际着色器内嵌在 D3D11Renderer.cpp 中编译

cbuffer Constants : register(b0) {
    float2 resolution;
    float2 padding;
};

Texture2D<float> texY : register(t0);
Texture2D<float> texU : register(t1);
Texture2D<float> texV : register(t2);
SamplerState sampler0 : register(s0);

struct VS_INPUT {
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_OUTPUT VS(VS_INPUT input) {
    VS_OUTPUT output;
    output.pos = float4(input.pos, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

float4 PS(VS_OUTPUT input) : SV_TARGET {
    float y = texY.Sample(sampler0, input.uv);
    float u = texU.Sample(sampler0, input.uv) - 0.5f;
    float v = texV.Sample(sampler0, input.uv) - 0.5f;

    // BT.601 YUV→RGB
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;

    return float4(r, g, b, 1.0);
}
