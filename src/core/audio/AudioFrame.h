#pragma once

#include <cstdint>
#include <vector>

struct AudioFrame {
    int sampleRate = 0;
    int channels = 0;
    int samples = 0;       // 采样数
    int bytesPerSample = 0;

    std::vector<uint8_t> data;
    double pts = 0.0;      // 时间戳 (秒)
};
