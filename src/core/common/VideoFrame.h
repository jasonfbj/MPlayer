#pragma once

#include <cstdint>
#include <vector>

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

    // YUV平面数据
    std::vector<uint8_t> data[3];  // Y, U, V
    int linesize[3] = {0, 0, 0};

    // 或 RGBA packed
    std::vector<uint8_t> rgbaData;

    // 平台纹理句柄 (硬解)
    void* nativeTexture = nullptr;

    double pts = 0.0;  // 显示时间戳 (秒)
    double duration = 0.0;
};
