#pragma once

#include <cstdint>
#include <vector>

// Hardware decode platform texture representation
struct NativeTexture {
    enum Type {
        D3D11_TEXTURE,              // Windows: ID3D11Texture2D*
        GL_TEXTURE_EXTERNAL_OES,    // Android: GLuint
        ANDROID_SURFACE_DIRECT      // Android: MediaCodec direct surface render
    };

    Type type = D3D11_TEXTURE;
    void* handle = nullptr;     // D3D11: ID3D11Texture2D*, Android: GLuint (cast)
    int width = 0;
    int height = 0;
    int index = 0;              // D3D11VA texture array index
};

struct VideoFrame {
    enum Format {
        YUV420P,
        NV12,
        RGB24,
        RGBA32,
        NativeTexture
    };

    Format format = YUV420P;
    int width = 0;
    int height = 0;

    // YUV planar data (software decode)
    std::vector<uint8_t> data[3];  // Y, U, V
    int linesize[3] = {0, 0, 0};

    // Or RGBA packed
    std::vector<uint8_t> rgbaData;

    // Platform texture (hardware decode)
    struct NativeTexture nativeTex;

    double pts = 0.0;           // Presentation timestamp (seconds)
    double duration = 0.0;
};
