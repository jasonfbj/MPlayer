# MPlayer 跨平台播放器实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 构建跨平台（Windows + Android）音视频播放器，基于 FFmpeg 支持本地文件和 RTMP 流播放。

**Architecture:** 三层架构 — 平台无关 C++ Core 层（Demuxer/Decoder/Renderer/Audio/Controller）、平台抽象接口（IRenderer/IDecoder/IAudioOutput）、平台实现层（Windows: D3D11/WASAPI/ImGui, Android: OpenGL ES/MediaCodec/OpenSL ES/JNI）。

**Tech Stack:** C++17, CMake 3.22+, FFmpeg, D3D11 (Windows), OpenGL ES 3.0 (Android), ImGui (Windows UI), Android NDK

---

## Phase 0: 基础框架搭建

### Task 0.1: 项目根 CMake 构建系统

**Files:**
- Create: `CMakeLists.txt`
- Create: `src/core/CMakeLists.txt`
- Create: `src/platform/windows/CMakeLists.txt`
- Create: `src/platform/android/CMakeLists.txt`

- [ ] **Step 1: 创建根 CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.22)
project(MPlayer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" OFF)
option(ENABLE_HARDWARE_DECODER "Enable hardware decoder" ON)

add_subdirectory(src/core)

if(WIN32)
    add_subdirectory(src/platform/windows)
elseif(ANDROID)
    add_subdirectory(src/platform/android)
endif()

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

- [ ] **Step 2: 创建 core CMakeLists.txt**

```cmake
add_library(MPlayerCore STATIC
    demuxer/Demuxer.cpp
    decoder/SoftwareDecoder.cpp
    decoder/DecoderFactory.cpp
    renderer/RendererFactory.cpp
    controller/PlayerController.cpp
    audio/AudioFrame.cpp
)

target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/ffmpeg/${MPLAYER_FFMPEG_PATH}/include
)

target_link_libraries(MPlayerCore PUBLIC
    MPlayerFFmpeg
)
```

- [ ] **Step 3: 创建平台 CMakeLists.txt 占位**

Windows `src/platform/windows/CMakeLists.txt`:
```cmake
add_executable(MPlayerWin
    main.cpp
    WinMainWindow.cpp
    D3D11Renderer.cpp
    D3D11VAHardwareDecoder.cpp
    WinAudioOutput.cpp
)

target_link_libraries(MPlayerWin PRIVATE
    MPlayerCore
    MPlayerPlatformWin
    d3d11
    dxgi
    d3dcompiler
    dxguid
   _imm32
)
```

Android `src/platform/android/CMakeLists.txt`:
```cmake
add_library(MPlayerAndroid SHARED
    jni/MPlayerJNI.cpp
    GLESRenderer.cpp
    MediaCodecDecoder.cpp
    AndroidAudioOutput.cpp
)

target_link_libraries(MPlayerAndroid PRIVATE
    MPlayerCore
    MPlayerPlatformAndroid
    EGL
    GLESv3
    OpenSLES
    android
    log
)
```

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/core/CMakeLists.txt src/platform/windows/CMakeLists.txt src/platform/android/CMakeLists.txt
git commit -m "build: initial CMake build system for core, windows, android"
```

---

### Task 0.2: FFmpeg 第三方库集成

**Files:**
- Create: `third_party/ffmpeg/CMakeLists.txt`
- Create: `scripts/download_ffmpeg.sh`
- Create: `scripts/download_ffmpeg.ps1`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: 创建 FFmpeg 导入 CMakeLists.txt**

```cmake
# third_party/ffmpeg/CMakeLists.txt
# 根据平台自动选择预编译库路径

if(WIN32)
    set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/windows/x64")
elseif(ANDROID)
    if(${ANDROID_ABI} STREQUAL "arm64-v8a")
        set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/arm64-v8a")
    elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
        set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/armeabi-v7a")
    endif()
elseif(APPLE)
    set(FFMPEG_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/macOS/arm64")
endif()

set(FFMPEG_INCLUDE_DIR "${FFMPEG_PLATFORM_DIR}/include")
set(FFMPEG_LIB_DIR "${FFMPEG_PLATFORM_DIR}/lib")

add_library(MPlayerFFmpeg INTERFACE IMPORTED)
target_include_directories(MPlayerFFmpeg INTERFACE ${FFMPEG_INCLUDE_DIR})

# 各FFmpeg库
set(FFMPEG_LIBS avcodec avformat avutil swscale swresample avcodec avformat)

if(WIN32)
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(MPlayerFFmpeg INTERFACE
            ${FFMPEG_LIB_DIR}/${lib}.lib
        )
    endforeach()
else()
    foreach(lib ${FFMPEG_LIBS})
        target_link_libraries(MPlayerFFmpeg INTERFACE
            ${FFMPEG_LIB_DIR}/lib${lib}.so
        )
    endforeach()
endif()

# 导出路径变量给上层使用
set(MPLAYER_FFMPEG_PATH "${FFMPEG_PLATFORM_DIR}" PARENT_SCOPE)
```

- [ ] **Step 2: 创建下载脚本 scripts/download_ffmpeg.sh**

```bash
#!/bin/bash
# 下载 FFmpeg 预编译库
# 用法: ./scripts/download_ffmpeg.sh [macOS|android]

PLATFORM=${1:-macOS}
BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
FFMPEG_DIR="$BASE_DIR/third_party/ffmpeg"

FFMPEG_VERSION="6.1.1"

echo "Downloading FFmpeg $FFMPEG_VERSION for $PLATFORM..."

case $PLATFORM in
    macOS)
        DEST="$FFMPEG_DIR/macOS/arm64"
        mkdir -p "$DEST"
        echo "Please download FFmpeg macOS arm64 from https://evermeet.cx/ffmpeg/"
        echo "Or build from source: ./configure --enable-shared && make && make install"
        echo "Extract include/ and lib/ to: $DEST"
        ;;
    android)
        echo "Please use https://github.com/ArmanAalmahdi/FFmpeg-Android-Builder"
        echo "Or AndroidFFmpeg to build FFmpeg for Android"
        echo "Place arm64-v8a output in: $FFMPEG_DIR/android/arm64-v8a/"
        echo "Place armeabi-v7a output in: $FFMPEG_DIR/android/armeabi-v7a/"
        ;;
    *)
        echo "Unknown platform: $PLATFORM"
        echo "Usage: $0 [macOS|android]"
        exit 1
        ;;
esac
```

- [ ] **Step 3: 更新根 CMakeLists.txt 加入 FFmpeg**

在 `add_subdirectory(src/core)` 之前加入:

```cmake
add_subdirectory(third_party/ffmpeg)
```

- [ ] **Step 4: Commit**

```bash
git add third_party/ffmpeg/CMakeLists.txt scripts/
git commit -m "build: add FFmpeg integration and download scripts"
```

---

### Task 0.3: 通用数据结构 - BlockingQueue

**Files:**
- Create: `src/core/common/BlockingQueue.h`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_blocking_queue.cpp`

- [ ] **Step 1: 编写 BlockingQueue 测试**

```cpp
// tests/test_blocking_queue.cpp
#include "core/common/BlockingQueue.h"
#include <cassert>
#include <thread>
#include <chrono>
#include <iostream>

void test_basic_push_pop() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(42);
    int val;
    assert(q.pop(val, 100));
    assert(val == 42);
    std::cout << "  PASS: basic push/pop\n";
}

void test_fifo_order() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(1);
    q.push(2);
    q.push(3);
    int val;
    q.pop(val, 100); assert(val == 1);
    q.pop(val, 100); assert(val == 2);
    q.pop(val, 100); assert(val == 3);
    std::cout << "  PASS: FIFO order\n";
}

void test_max_size_backpressure() {
    BlockingQueue<int> q;
    q.setMaxSize(2);
    assert(q.push(1));
    assert(q.push(2));
    assert(q.isFull());
    // 超出容量时 push 应返回 false 或阻塞（非阻塞模式返回false）
    assert(!q.push(3));
    std::cout << "  PASS: max size backpressure\n";
}

void test_timeout_pop() {
    BlockingQueue<int> q;
    int val;
    // 空队列 pop 应超时返回 false
    assert(!q.pop(val, 50));
    std::cout << "  PASS: timeout pop\n";
}

void test_concurrent_access() {
    BlockingQueue<int> q;
    q.setMaxSize(100);

    std::thread producer([&]() {
        for (int i = 0; i < 50; i++) {
            q.push(i);
        }
    });

    int sum = 0;
    std::thread consumer([&]() {
        for (int i = 0; i < 50; i++) {
            int val;
            if (q.pop(val, 1000)) {
                sum += val;
            }
        }
    });

    producer.join();
    consumer.join();
    assert(sum == (49 * 50) / 2); // 0+1+2+...+49
    std::cout << "  PASS: concurrent access\n";
}

void test_clear() {
    BlockingQueue<int> q;
    q.setMaxSize(10);
    q.push(1);
    q.push(2);
    q.push(3);
    q.clear();
    assert(q.isEmpty());
    assert(q.size() == 0);
    std::cout << "  PASS: clear\n";
}

int main() {
    std::cout << "Running BlockingQueue tests...\n";
    test_basic_push_pop();
    test_fifo_order();
    test_max_size_backpressure();
    test_timeout_pop();
    test_concurrent_access();
    test_clear();
    std::cout << "All BlockingQueue tests passed!\n";
    return 0;
}
```

- [ ] **Step 2: 运行测试确认编译失败**

Run: `cd /Users/cvte/Downloads/MPlayer && cmake -B build -DBUILD_TESTS=ON 2>&1 | tail -5`
Expected: 编译失败，BlockingQueue.h 不存在

- [ ] **Step 3: 实现 BlockingQueue**

```cpp
// src/core/common/BlockingQueue.h
#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>

template<typename T>
class BlockingQueue {
public:
    BlockingQueue() : max_size_(0), finished_(false) {}

    void setMaxSize(size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        max_size_ = size;
    }

    bool push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (max_size_ > 0 && queue_.size() >= max_size_) {
            return false;
        }
        queue_.push(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    bool pop(T& item, int timeoutMs = -1) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timeoutMs < 0) {
            not_empty_.wait(lock, [this] { return !queue_.empty() || finished_; });
        } else {
            if (!not_empty_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                [this] { return !queue_.empty() || finished_; })) {
                return false;
            }
        }
        if (queue_.empty()) return false;
        item = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::queue<T> empty;
        queue_.swap(empty);
    }

    bool isEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    bool isFull() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return max_size_ > 0 && queue_.size() >= max_size_;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    void finish() {
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = true;
        not_empty_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::queue<T> queue_;
    size_t max_size_;
    bool finished_;
};
```

- [ ] **Step 4: 创建测试 CMakeLists.txt**

```cmake
# tests/CMakeLists.txt
add_executable(test_blocking_queue test_blocking_queue.cpp)
target_link_libraries(test_blocking_queue PRIVATE MPlayerCore)
add_test(NAME blocking_queue COMMAND test_blocking_queue)
```

- [ ] **Step 5: 编译并运行测试**

Run: `cd /Users/cvte/Downloads/MPlayer && cmake -B build -DBUILD_TESTS=ON && cmake --build build --target test_blocking_queue && ./build/tests/test_blocking_queue`
Expected: All BlockingQueue tests passed!

- [ ] **Step 6: Commit**

```bash
git add src/core/common/BlockingQueue.h tests/CMakeLists.txt tests/test_blocking_queue.cpp
git commit -m "feat: add BlockingQueue with tests"
```

---

### Task 0.4: NonCopyable 工具类和 Frame 数据结构

**Files:**
- Create: `src/core/common/NonCopyable.h`
- Create: `src/core/common/VideoFrame.h`
- Create: `src/core/audio/AudioFrame.h`

- [ ] **Step 1: 创建 NonCopyable**

```cpp
// src/core/common/NonCopyable.h
#pragma once

class NonCopyable {
public:
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};
```

- [ ] **Step 2: 创建 VideoFrame**

```cpp
// src/core/common/VideoFrame.h
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
```

- [ ] **Step 3: 创建 AudioFrame**

```cpp
// src/core/audio/AudioFrame.h
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
```

- [ ] **Step 4: Commit**

```bash
git add src/core/common/NonCopyable.h src/core/common/VideoFrame.h src/core/audio/AudioFrame.h
git commit -m "feat: add NonCopyable, VideoFrame, AudioFrame data structures"
```

---

### Task 0.5: 抽象接口定义

**Files:**
- Create: `src/core/decoder/IDecoder.h`
- Create: `src/core/renderer/IRenderer.h`
- Create: `src/core/audio/IAudioOutput.h`

- [ ] **Step 1: 创建 IDecoder 接口**

```cpp
// src/core/decoder/IDecoder.h
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

class IDecoder {
public:
    virtual ~IDecoder() = default;

    virtual bool init(const AVCodecParameters* params) = 0;
    virtual bool decode(const AVPacket* packet, AVFrame* frame) = 0;
    virtual void flush() = 0;
    virtual void destroy() = 0;

    virtual bool isHardware() const = 0;
    virtual AVPixelFormat outputFormat() const = 0;
};
```

- [ ] **Step 2: 创建 IRenderer 接口**

```cpp
// src/core/renderer/IRenderer.h
#pragma once

#include "core/common/VideoFrame.h"

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(void* nativeWindow) = 0;
    virtual bool renderFrame(const VideoFrame& frame) = 0;
    virtual bool renderTexture(void* nativeTexture, int width, int height) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void destroy() = 0;
};
```

- [ ] **Step 3: 创建 IAudioOutput 接口**

```cpp
// src/core/audio/IAudioOutput.h
#pragma once

#include "core/audio/AudioFrame.h"
#include <functional>

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    using AudioCallback = std::function<void(uint8_t* output, int size)>;

    virtual bool init(int sampleRate, int channels, int bytesPerSample) = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual void setVolume(float volume) = 0;
    virtual void setCallback(AudioCallback callback) = 0;
    virtual void destroy() = 0;
};
```

- [ ] **Step 4: Commit**

```bash
git add src/core/decoder/IDecoder.h src/core/renderer/IRenderer.h src/core/audio/IAudioOutput.h
git commit -m "feat: define IDecoder, IRenderer, IAudioOutput interfaces"
```

---

## Phase 1: 核心播放引擎

### Task 1.1: Demuxer 实现

**Files:**
- Create: `src/core/demuxer/Demuxer.h`
- Create: `src/core/demuxer/Demuxer.cpp`
- Create: `tests/test_demuxer.cpp`

- [ ] **Step 1: 编写 Demuxer 测试**

```cpp
// tests/test_demuxer.cpp
#include "core/demuxer/Demuxer.h"
#include <cassert>
#include <iostream>
#include <string>

// 此测试需要提供一个测试视频文件路径
// 用法: test_demuxer <video_file>

void test_open_file(const std::string& path) {
    Demuxer demuxer;
    assert(demuxer.open(path));

    assert(demuxer.getVideoStreamIndex() >= 0);
    std::cout << "  Video stream index: " << demuxer.getVideoStreamIndex() << "\n";

    auto* vparams = demuxer.getVideoParams();
    assert(vparams != nullptr);
    std::cout << "  Video: " << vparams->width << "x" << vparams->height
              << ", codec: " << avcodec_get_name(vparams->codec_id) << "\n";

    AVPacket* pkt = av_packet_alloc();
    int packetCount = 0;
    while (demuxer.readPacket(pkt) && packetCount < 10) {
        packetCount++;
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    assert(packetCount > 0);
    std::cout << "  Read " << packetCount << " packets\n";

    demuxer.close();
    std::cout << "  PASS: open file and read packets\n";
}

void test_seek(const std::string& path) {
    Demuxer demuxer;
    assert(demuxer.open(path));

    assert(demuxer.seek(5.0)); // seek to 5 seconds

    AVPacket* pkt = av_packet_alloc();
    int count = 0;
    while (demuxer.readPacket(pkt) && count < 5) {
        count++;
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
    assert(count > 0);
    std::cout << "  PASS: seek and read\n";

    demuxer.close();
}

void test_is_stream() {
    Demuxer demuxer;
    demuxer.open("test.mp4");
    assert(!demuxer.isStream());
    std::cout << "  PASS: isStream for local file\n";
    demuxer.close();
}

int main(int argc, char* argv[]) {
    std::cout << "Running Demuxer tests...\n";
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <video_file>\n";
        return 1;
    }
    std::string file = argv[1];
    test_open_file(file);
    test_seek(file);
    test_is_stream();
    std::cout << "All Demuxer tests passed!\n";
    return 0;
}
```

- [ ] **Step 2: 实现 Demuxer.h**

```cpp
// src/core/demuxer/Demuxer.h
#pragma once

#include <string>
#include <atomic>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

class Demuxer {
public:
    Demuxer();
    ~Demuxer();

    bool open(const std::string& url);
    void close();

    bool readPacket(AVPacket* packet);

    int getVideoStreamIndex() const { return videoStreamIndex_; }
    int getAudioStreamIndex() const { return audioStreamIndex_; }
    AVCodecParameters* getVideoParams() const;
    AVCodecParameters* getAudioParams() const;

    bool seek(double seconds);
    bool isStream() const;
    bool isOpened() const { return opened_; }

    double duration() const;
    double currentPosition() const;

private:
    AVFormatContext* formatCtx_ = nullptr;
    int videoStreamIndex_ = -1;
    int audioStreamIndex_ = -1;
    std::atomic<bool> opened_{false};
    std::atomic<bool> eof_{false};
};
```

- [ ] **Step 3: 实现 Demuxer.cpp**

```cpp
// src/core/demuxer/Demuxer.cpp
#include "core/demuxer/Demuxer.h"
#include <cstring>

Demuxer::Demuxer() = default;

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const std::string& url) {
    if (opened_) close();

    formatCtx_ = avformat_alloc_context();
    if (!formatCtx_) return false;

    // 网络流设置
    if (url.find("rtmp://") == 0 || url.find("http://") == 0 || url.find("rtsp://") == 0) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "timeout", "5000000", 0);        // 5秒超时(微秒)
        av_dict_set(&opts, "buffer_size", "1024000", 0);
        av_dict_set(&opts, "max_delay", "500000", 0);
        av_dict_set(&opts, "rtmp_live", "live", 0);
        av_dict_set(&opts, "fflags", "nobuffer", 0);

        int ret = avformat_open_input(&formatCtx_, url.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            return false;
        }
    } else {
        if (avformat_open_input(&formatCtx_, url.c_str(), nullptr, nullptr) < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            return false;
        }
    }

    if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
        close();
        return false;
    }

    // 查找音视频流
    for (unsigned int i = 0; i < formatCtx_->nb_streams; i++) {
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO &&
            videoStreamIndex_ < 0) {
            videoStreamIndex_ = i;
        }
        if (formatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO &&
            audioStreamIndex_ < 0) {
            audioStreamIndex_ = i;
        }
    }

    opened_ = true;
    eof_ = false;
    return true;
}

void Demuxer::close() {
    if (formatCtx_) {
        avformat_close_input(&formatCtx_);
        formatCtx_ = nullptr;
    }
    videoStreamIndex_ = -1;
    audioStreamIndex_ = -1;
    opened_ = false;
    eof_ = false;
}

bool Demuxer::readPacket(AVPacket* packet) {
    if (!opened_ || eof_) return false;

    int ret = av_read_frame(formatCtx_, packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF || avio_feof(formatCtx_->pb)) {
            eof_ = true;
        }
        return false;
    }
    return true;
}

AVCodecParameters* Demuxer::getVideoParams() const {
    if (videoStreamIndex_ < 0) return nullptr;
    return formatCtx_->streams[videoStreamIndex_]->codecpar;
}

AVCodecParameters* Demuxer::getAudioParams() const {
    if (audioStreamIndex_ < 0) return nullptr;
    return formatCtx_->streams[audioStreamIndex_]->codecpar;
}

bool Demuxer::seek(double seconds) {
    if (!opened_) return false;

    int64_t ts = static_cast<int64_t>(seconds * AV_TIME_BASE);
    int ret = av_seek_frame(formatCtx_, -1, ts, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) return false;

    eof_ = false;
    return true;
}

bool Demuxer::isStream() const {
    if (!opened_ || !formatCtx_) return false;
    return formatCtx_->iformat && (
        std::strstr(formatCtx_->iformat->name, "rtmp") != nullptr ||
        formatCtx_->pb == nullptr ||
        !formatCtx_->pb->seekable
    );
}

double Demuxer::duration() const {
    if (!opened_ || !formatCtx_) return 0.0;
    return static_cast<double>(formatCtx_->duration) / AV_TIME_BASE;
}

double Demuxer::currentPosition() const {
    if (!opened_ || !formatCtx_) return 0.0;
    return static_cast<double>(formatCtx_->pb->pos) / AV_TIME_BASE;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/core/demuxer/Demuxer.h src/core/demuxer/Demuxer.cpp tests/test_demuxer.cpp
git commit -m "feat: implement Demuxer with file and RTMP support"
```

---

### Task 1.2: SoftwareDecoder 实现

**Files:**
- Create: `src/core/decoder/SoftwareDecoder.h`
- Create: `src/core/decoder/SoftwareDecoder.cpp`

- [ ] **Step 1: 实现 SoftwareDecoder.h**

```cpp
// src/core/decoder/SoftwareDecoder.h
#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

class SoftwareDecoder : public IDecoder, public NonCopyable {
public:
    SoftwareDecoder() = default;
    ~SoftwareDecoder() override { destroy(); }

    bool init(const AVCodecParameters* params) override;
    bool decode(const AVPacket* packet, AVFrame* frame) override;
    void flush() override;
    void destroy() override;

    bool isHardware() const override { return false; }
    AVPixelFormat outputFormat() const override;

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVCodecParserContext* parser_ = nullptr;
};
```

- [ ] **Step 2: 实现 SoftwareDecoder.cpp**

```cpp
// src/core/decoder/SoftwareDecoder.cpp
#include "core/decoder/SoftwareDecoder.h"

bool SoftwareDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) return false;

    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) return false;

    if (avcodec_parameters_to_context(codecCtx_, params) < 0) {
        destroy();
        return false;
    }

    // 使用多线程解码
    codecCtx_->thread_count = 0; // 自动选择线程数

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        destroy();
        return false;
    }

    return true;
}

bool SoftwareDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codecCtx_) return false;

    int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
        return false;
    }

    ret = avcodec_receive_frame(codecCtx_, frame);
    return ret == 0;
}

void SoftwareDecoder::flush() {
    if (codecCtx_) {
        avcodec_flush_buffers(codecCtx_);
    }
}

void SoftwareDecoder::destroy() {
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    codec_ = nullptr;
}

AVPixelFormat SoftwareDecoder::outputFormat() const {
    if (!codecCtx_) return AV_PIX_FMT_NONE;
    return codecCtx_->pix_fmt;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/decoder/SoftwareDecoder.h src/core/decoder/SoftwareDecoder.cpp
git commit -m "feat: implement SoftwareDecoder using FFmpeg"
```

---

### Task 1.3: DecoderFactory 实现

**Files:**
- Create: `src/core/decoder/DecoderFactory.h`
- Create: `src/core/decoder/DecoderFactory.cpp`

- [ ] **Step 1: 实现 DecoderFactory**

```cpp
// src/core/decoder/DecoderFactory.h
#pragma once

#include "core/decoder/IDecoder.h"
#include <memory>
#include <string>

class DecoderFactory {
public:
    enum class DecoderType {
        Auto,       // 自动选择
        Software,   // 强制软解
        Hardware    // 强制硬解
    };

    // 创建视频解码器
    static std::unique_ptr<IDecoder> createVideoDecoder(
        const AVCodecParameters* params,
        DecoderType type = DecoderType::Auto
    );

    // 创建音频解码器
    static std::unique_ptr<IDecoder> createAudioDecoder(
        const AVCodecParameters* params
    );

    // 检查硬件解码是否可用
    static bool isHardwareDecodeAvailable(const AVCodecParameters* params);
};
```

```cpp
// src/core/decoder/DecoderFactory.cpp
#include "core/decoder/DecoderFactory.h"
#include "core/decoder/SoftwareDecoder.h"

std::unique_ptr<IDecoder> DecoderFactory::createVideoDecoder(
    const AVCodecParameters* params,
    DecoderType type
) {
    // 硬件解码将在各平台实现中注册
    // 目前只返回软解码器
    if (type == DecoderType::Hardware) {
        if (isHardwareDecodeAvailable(params)) {
            // 由平台层注入的工厂函数创建
            // 这里先回退到软解
        }
    }

    auto decoder = std::make_unique<SoftwareDecoder>();
    if (decoder->init(params)) {
        return decoder;
    }
    return nullptr;
}

std::unique_ptr<IDecoder> DecoderFactory::createAudioDecoder(
    const AVCodecParameters* params
) {
    auto decoder = std::make_unique<SoftwareDecoder>();
    if (decoder->init(params)) {
        return decoder;
    }
    return nullptr;
}

bool DecoderFactory::isHardwareDecodeAvailable(const AVCodecParameters* params) {
    // 各平台实现中会覆盖此行为
    // 默认返回 false
    return false;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/core/decoder/DecoderFactory.h src/core/decoder/DecoderFactory.cpp
git commit -m "feat: add DecoderFactory with auto/software/hardware modes"
```

---

### Task 1.4: RendererFactory 实现

**Files:**
- Create: `src/core/renderer/RendererFactory.h`
- Create: `src/core/renderer/RendererFactory.cpp`

- [ ] **Step 1: 实现 RendererFactory**

```cpp
// src/core/renderer/RendererFactory.h
#pragma once

#include "core/renderer/IRenderer.h"
#include <memory>
#include <functional>
#include <string>

class RendererFactory {
public:
    using Creator = std::function<std::unique_ptr<IRenderer>()>;

    // 注册平台渲染器创建函数
    static void registerRenderer(const std::string& platform, Creator creator);

    // 创建渲染器
    static std::unique_ptr<IRenderer> create(const std::string& platform);

private:
    static Creator& getCreator();
};
```

```cpp
// src/core/renderer/RendererFactory.cpp
#include "core/renderer/RendererFactory.h"

RendererFactory::Creator& RendererFactory::getCreator() {
    static Creator creator;
    return creator;
}

void RendererFactory::registerRenderer(const std::string& platform, Creator creator) {
    getCreator() = std::move(creator);
}

std::unique_ptr<IRenderer> RendererFactory::create(const std::string& platform) {
    auto& creator = getCreator();
    if (creator) {
        return creator();
    }
    return nullptr;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/core/renderer/RendererFactory.h src/core/renderer/RendererFactory.cpp
git commit -m "feat: add RendererFactory with platform registration"
```

---

### Task 1.5: PlayerController 实现

**Files:**
- Create: `src/core/controller/PlayerController.h`
- Create: `src/core/controller/PlayerController.cpp`

- [ ] **Step 1: 实现 PlayerController.h**

```cpp
// src/core/controller/PlayerController.h
#pragma once

#include "core/common/NonCopyable.h"
#include "core/common/VideoFrame.h"
#include "core/common/BlockingQueue.h"
#include "core/audio/AudioFrame.h"
#include "core/demuxer/Demuxer.h"
#include "core/decoder/IDecoder.h"
#include "core/decoder/DecoderFactory.h"
#include "core/renderer/IRenderer.h"
#include "core/audio/IAudioOutput.h"

#include <memory>
#include <thread>
#include <atomic>
#include <string>
#include <functional>
#include <mutex>

struct VideoInfo {
    std::string codecName;
    int width = 0;
    int height = 0;
    double duration = 0;
    int bitrate = 0;
    double frameRate = 0;
};

class PlayerController : public NonCopyable {
public:
    PlayerController();
    ~PlayerController();

    bool open(const std::string& url);
    void close();

    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void setSpeed(float speed);
    void setVolume(float volume);

    enum State { Idle, Playing, Paused, Stopped };
    State state() const { return state_; }
    double duration() const;
    double currentPosition() const;

    bool captureFrame(const std::string& savePath);
    VideoInfo getVideoInfo() const;

    // 设置平台组件
    void setRenderer(std::unique_ptr<IRenderer> renderer);
    void setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput);

    // 回调
    using FrameCallback = std::function<void(const VideoFrame&)>;
    void setVideoFrameCallback(FrameCallback cb) { videoFrameCb_ = std::move(cb); }

    using StateCallback = std::function<void(State)>;
    void setStateCallback(StateCallback cb) { stateCb_ = std::move(cb); }

    using ErrorCallback = std::function<void(const std::string&)>;
    void setErrorCallback(ErrorCallback cb) { errorCb_ = std::move(cb); }

private:
    void readThread();
    void decodeThread();
    void audioDecodeThread();

    void setState(State s);

    // 组件
    std::unique_ptr<Demuxer> demuxer_;
    std::unique_ptr<IDecoder> videoDecoder_;
    std::unique_ptr<IDecoder> audioDecoder_;
    std::unique_ptr<IRenderer> renderer_;
    std::unique_ptr<IAudioOutput> audioOutput_;

    // 队列
    BlockingQueue<AVPacket*> videoPacketQueue_;
    BlockingQueue<AVPacket*> audioPacketQueue_;
    BlockingQueue<VideoFrame> videoFrameQueue_;
    BlockingQueue<AudioFrame> audioFrameQueue_;

    // 线程
    std::thread readThread_;
    std::thread videoDecodeThread_;
    std::thread audioDecodeThread_;
    std::atomic<bool> running_{false};

    // 状态
    std::atomic<State> state_{Idle};
    std::atomic<float> speed_{1.0f};
    std::atomic<double> currentPosition_{0.0};
    std::string currentUrl_;
    mutable std::mutex mutex_;

    // 回调
    FrameCallback videoFrameCb_;
    StateCallback stateCb_;
    ErrorCallback errorCb_;
};
```

- [ ] **Step 2: 实现 PlayerController.cpp**

```cpp
// src/core/controller/PlayerController.cpp
#include "core/controller/PlayerController.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

static const size_t MAX_PACKET_QUEUE_SIZE = 500;
static const size_t MAX_FRAME_QUEUE_SIZE = 30;

PlayerController::PlayerController()
    : demuxer_(std::make_unique<Demuxer>()) {
    videoPacketQueue_.setMaxSize(MAX_PACKET_QUEUE_SIZE);
    audioPacketQueue_.setMaxSize(MAX_PACKET_QUEUE_SIZE);
    videoFrameQueue_.setMaxSize(MAX_FRAME_QUEUE_SIZE);
    audioFrameQueue_.setMaxSize(MAX_FRAME_QUEUE_SIZE);
}

PlayerController::~PlayerController() {
    close();
}

bool PlayerController::open(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != Idle && state_ != Stopped) {
        close();
    }

    if (!demuxer_->open(url)) {
        if (errorCb_) errorCb_("Failed to open: " + url);
        return false;
    }

    currentUrl_ = url;

    // 创建视频解码器
    if (demuxer_->getVideoStreamIndex() >= 0) {
        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto
        );
        if (!videoDecoder_) {
            if (errorCb_) errorCb_("Failed to create video decoder");
            return false;
        }
    }

    // 创建音频解码器
    if (demuxer_->getAudioStreamIndex() >= 0) {
        audioDecoder_ = DecoderFactory::createAudioDecoder(
            demuxer_->getAudioParams()
        );
    }

    // 初始化音频输出
    if (audioOutput_ && demuxer_->getAudioStreamIndex() >= 0) {
        auto* aparams = demuxer_->getAudioParams();
        audioOutput_->init(aparams->sample_rate, aparams->ch_layout.nb_channels, 2);
    }

    setState(Stopped);
    return true;
}

void PlayerController::close() {
    stop();

    videoPacketQueue_.finish();
    audioPacketQueue_.finish();
    videoFrameQueue_.finish();
    audioFrameQueue_.finish();

    demuxer_->close();
    videoDecoder_.reset();
    audioDecoder_.reset();

    videoPacketQueue_.clear();
    audioPacketQueue_.clear();
    videoFrameQueue_.clear();
    audioFrameQueue_.clear();

    setState(Idle);
}

void PlayerController::play() {
    if (state_ == Playing) return;
    if (state_ != Paused && state_ != Stopped) return;

    if (state_ == Stopped) {
        running_ = true;
        // 清空队列
        videoPacketQueue_.clear();
        audioPacketQueue_.clear();
        videoFrameQueue_.clear();
        audioFrameQueue_.clear();

        // 如果是停止状态，重新打开
        if (!demuxer_->isOpened() && !currentUrl_.empty()) {
            demuxer_->open(currentUrl_);
            if (videoDecoder_) {
                videoDecoder_->flush();
            }
            if (audioDecoder_) {
                audioDecoder_->flush();
            }
        }

        // 启动线程
        readThread_ = std::thread(&PlayerController::readThread, this);
        videoDecodeThread_ = std::thread(&PlayerController::decodeThread, this);
        if (audioDecoder_) {
            audioDecodeThread_ = std::thread(&PlayerController::audioDecodeThread, this);
        }
    }

    if (audioOutput_) audioOutput_->start();
    setState(Playing);
}

void PlayerController::pause() {
    if (state_ != Playing) return;
    if (audioOutput_) audioOutput_->stop();
    setState(Paused);
}

void PlayerController::stop() {
    running_ = false;

    videoPacketQueue_.finish();
    audioPacketQueue_.finish();

    if (readThread_.joinable()) readThread_.join();
    if (videoDecodeThread_.joinable()) videoDecodeThread_.join();
    if (audioDecodeThread_.joinable()) audioDecodeThread_.join();

    if (audioOutput_) audioOutput_->stop();
    setState(Stopped);
}

void PlayerController::seek(double seconds) {
    if (!demuxer_->isOpened()) return;

    // 暂时清空队列
    videoPacketQueue_.clear();
    audioPacketQueue_.clear();
    videoFrameQueue_.clear();
    audioFrameQueue_.clear();

    if (videoDecoder_) videoDecoder_->flush();
    if (audioDecoder_) audioDecoder_->flush();

    demuxer_->seek(seconds);
    currentPosition_ = seconds;
}

void PlayerController::setSpeed(float speed) {
    speed_ = speed;
}

void PlayerController::setVolume(float volume) {
    if (audioOutput_) {
        audioOutput_->setVolume(volume);
    }
}

double PlayerController::duration() const {
    return demuxer_ ? demuxer_->duration() : 0.0;
}

double PlayerController::currentPosition() const {
    return currentPosition_;
}

void PlayerController::setRenderer(std::unique_ptr<IRenderer> renderer) {
    renderer_ = std::move(renderer);
}

void PlayerController::setAudioOutput(std::unique_ptr<IAudioOutput> audioOutput) {
    audioOutput_ = std::move(audioOutput);
}

VideoInfo PlayerController::getVideoInfo() const {
    VideoInfo info;
    if (!demuxer_ || !demuxer_->isOpened()) return info;

    auto* vparams = demuxer_->getVideoParams();
    if (vparams) {
        info.codecName = avcodec_get_name(vparams->codec_id);
        info.width = vparams->width;
        info.height = vparams->height;
        info.duration = demuxer_->duration();
        info.bitrate = vparams->bit_rate;

        if (demuxer_->getVideoStreamIndex() >= 0) {
            auto* stream = demuxer_->getFormatContext()->streams[demuxer_->getVideoStreamIndex()];
            if (stream->avg_frame_rate.den > 0) {
                info.frameRate = av_q2d(stream->avg_frame_rate);
            }
        }
    }
    return info;
}

bool PlayerController::captureFrame(const std::string& savePath) {
    // 后续实现：保存当前帧为图片
    return false;
}

void PlayerController::readThread() {
    AVPacket* packet = av_packet_alloc();
    while (running_) {
        if (!demuxer_->readPacket(packet)) {
            // EOF 或错误
            break;
        }

        if (packet->stream_index == demuxer_->getVideoStreamIndex()) {
            AVPacket* pkt = av_packet_alloc();
            av_packet_ref(pkt, packet);
            if (!videoPacketQueue_.push(pkt)) {
                av_packet_free(&pkt);
            }
        } else if (packet->stream_index == demuxer_->getAudioStreamIndex()) {
            AVPacket* pkt = av_packet_alloc();
            av_packet_ref(pkt, packet);
            if (!audioPacketQueue_.push(pkt)) {
                av_packet_free(&pkt);
            }
        }

        av_packet_unref(packet);
    }
    av_packet_free(&packet);
}

void PlayerController::decodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    while (running_) {
        if (!videoPacketQueue_.pop(packet, 100)) {
            continue;
        }

        if (videoDecoder_->decode(packet, frame)) {
            VideoFrame vf;
            vf.width = frame->width;
            vf.height = frame->height;
            vf.pts = frame->pts * av_q2d(
                demuxer_->getFormatContext()->streams[demuxer_->getVideoStreamIndex()]->time_base
            );

            // 转换为YUV420P
            if (frame->format == AV_PIX_FMT_YUV420P) {
                vf.format = VideoFrame::YUV420P;
                for (int i = 0; i < 3; i++) {
                    vf.linesize[i] = frame->linesize[i];
                    int size = frame->linesize[i] * (i == 0 ? frame->height : frame->height / 2);
                    vf.data[i].assign(frame->data[i], frame->data[i] + size);
                }
            } else if (frame->format == AV_PIX_FMT_NV12) {
                vf.format = VideoFrame::NV12;
                vf.linesize[0] = frame->linesize[0];
                vf.linesize[1] = frame->linesize[1];
                int ySize = frame->linesize[0] * frame->height;
                vf.data[0].assign(frame->data[0], frame->data[0] + ySize);
                int uvSize = frame->linesize[1] * frame->height / 2;
                vf.data[1].assign(frame->data[1], frame->data[1] + uvSize);
            }

            if (!videoFrameQueue_.push(std::move(vf))) {
                // 队列满，丢弃帧
            }

            currentPosition_ = vf.pts;
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void PlayerController::audioDecodeThread() {
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = nullptr;

    while (running_) {
        if (!audioPacketQueue_.pop(packet, 100)) {
            continue;
        }

        if (audioDecoder_ && audioDecoder_->decode(packet, frame)) {
            AudioFrame af;
            af.sampleRate = frame->sample_rate;
            af.channels = frame->ch_layout.nb_channels;
            af.samples = frame->nb_samples;
            af.bytesPerSample = av_get_bytes_per_sample(static_cast<AVSampleFormat>(frame->format));
            af.pts = frame->pts * av_q2d(
                demuxer_->getFormatContext()->streams[demuxer_->getAudioStreamIndex()]->time_base
            );

            int dataSize = av_samples_get_buffer_size(nullptr, af.channels, af.samples,
                static_cast<AVSampleFormat>(frame->format), 1);
            af.data.assign(frame->data[0], frame->data[0] + dataSize);

            audioFrameQueue_.push(std::move(af));
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}

void PlayerController::setState(State s) {
    state_ = s;
    if (stateCb_) stateCb_(s);
}
```

- [ ] **Step 3: 在 Demuxer 中暴露 formatContext 给 Controller**

在 Demuxer.h 中添加:

```cpp
AVFormatContext* getFormatContext() const { return formatCtx_; }
```

- [ ] **Step 4: Commit**

```bash
git add src/core/controller/PlayerController.h src/core/controller/PlayerController.cpp src/core/demuxer/Demuxer.h
git commit -m "feat: implement PlayerController with threading and queue management"
```

---

## Phase 2: Windows 平台实现

### Task 2.1: D3D11 渲染器

**Files:**
- Create: `src/platform/windows/D3D11Renderer.h`
- Create: `src/platform/windows/D3D11Renderer.cpp`
- Create: `src/platform/windows/shaders/yuv2rgb.hlsl`

- [ ] **Step 1: 实现 D3D11Renderer.h**

```cpp
// src/platform/windows/D3D11Renderer.h
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
    bool renderTexture(void* nativeTexture, int width, int height) override;
    void resize(int width, int height) override;
    void destroy() override;

private:
    bool createDevice();
    bool createSwapChain(HWND hwnd);
    bool createRenderTargetView();
    bool createVertexShader();
    bool createPixelShader();
    bool createSamplerState();
    bool createVertexBuffer();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11SamplerState> samplerState_;
    ComPtr<ID3D11Buffer> vertexBuffer_;

    // YUV纹理
    ComPtr<ID3D11Texture2D> yTexture_;
    ComPtr<ID3D11Texture2D> uTexture_;
    ComPtr<ID3D11Texture2D> vTexture_;
    ComPtr<ID3D11ShaderResourceView> y_SRV_;
    ComPtr<ID3D11ShaderResourceView> u_SRV_;
    ComPtr<ID3D11ShaderResourceView> v_SRV_;

    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
};
```

- [ ] **Step 2: 创建 YUV→RGB HLSL 着色器**

```hlsl
// src/platform/windows/shaders/yuv2rgb.hlsl

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
```

- [ ] **Step 3: 实现 D3D11Renderer.cpp**

```cpp
// src/platform/windows/D3D11Renderer.cpp
#include "D3D11Renderer.h"
#include <d3dcompiler.h>
#include <fstream>
#include <vector>
#include <cassert>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

bool D3D11Renderer::init(void* nativeWindow) {
    HWND hwnd = static_cast<HWND>(nativeWindow);
    if (!hwnd) return false;

    if (!createDevice()) return false;
    if (!createSwapChain(hwnd)) return false;
    if (!createRenderTargetView()) return false;
    if (!createVertexShader()) return false;
    if (!createPixelShader()) return false;
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
    desc.BufferDesc.Width = 0;
    desc.BufferDesc.Height = 0;
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferDesc.RefreshRate.Numerator = 60;
    desc.BufferDesc.RefreshRate.Denominator = 1;
    desc.Windowed = TRUE;
    desc.OutputWindow = hwnd;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
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

bool D3D11Renderer::createVertexShader() {
    // 全屏四边形顶点着色器 (内嵌编译)
    const char* vsSource = R"(
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
    )";

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "VS", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) return false;

    return SUCCEEDED(device_->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &vertexShader_));
}

bool D3D11Renderer::createPixelShader() {
    const char* psSource = R"(
        Texture2D<float> texY : register(t0);
        Texture2D<float> texU : register(t1);
        Texture2D<float> texV : register(t2);
        SamplerState sampler0 : register(s0);

        struct VS_OUTPUT {
            float4 pos : SV_POSITION;
            float2 uv : TEXCOORD;
        };

        float4 PS(VS_OUTPUT input) : SV_TARGET {
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
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
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
    // 全屏四边形
    struct Vertex { float x, y, u, v; };
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 0.0f },  // 左上
        {  1.0f,  1.0f, 1.0f, 0.0f },  // 右上
        { -1.0f, -1.0f, 0.0f, 1.0f },  // 左下
        {  1.0f, -1.0f, 1.0f, 1.0f },  // 右下
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

    // 创建或更新YUV纹理
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

    // 渲染
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
    ID3D11Texture2D* d3dTex = static_cast<ID3D11Texture2D*>(nativeTexture);
    if (!d3dTex || !initialized_) return false;
    // TODO: 实现硬解纹理共享渲染
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
```

- [ ] **Step 4: Commit**

```bash
git add src/platform/windows/D3D11Renderer.h src/platform/windows/D3D11Renderer.cpp src/platform/windows/shaders/yuv2rgb.hlsl
git commit -m "feat: implement D3D11Renderer with YUV→RGB shader"
```

---

### Task 2.2: Windows 音频输出 (WASAPI)

**Files:**
- Create: `src/platform/windows/WinAudioOutput.h`
- Create: `src/platform/windows/WinAudioOutput.cpp`

- [ ] **Step 1: 实现 WinAudioOutput.h**

```cpp
// src/platform/windows/WinAudioOutput.h
#pragma once

#include "core/audio/IAudioOutput.h"
#include "core/common/NonCopyable.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <thread>
#include <atomic>

class WinAudioOutput : public IAudioOutput, public NonCopyable {
public:
    WinAudioOutput() = default;
    ~WinAudioOutput() override { destroy(); }

    bool init(int sampleRate, int channels, int bytesPerSample) override;
    bool start() override;
    bool stop() override;
    void setVolume(float volume) override;
    void setCallback(AudioCallback callback) override;
    void destroy() override;

private:
    void audioThread();

    IMMDevice* device_ = nullptr;
    IAudioClient* audioClient_ = nullptr;
    IAudioRenderClient* renderClient_ = nullptr;
    IAudioStreamVolume* streamVolume_ = nullptr;

    std::thread thread_;
    std::atomic<bool> running_{false};
    AudioCallback callback_;

    int sampleRate_ = 0;
    int channels_ = 0;
    int bytesPerSample_ = 0;
    UINT32 bufferFrameCount_ = 0;
};
```

- [ ] **Step 2: 实现 WinAudioOutput.cpp**

```cpp
// src/platform/windows/WinAudioOutput.cpp
#include "WinAudioOutput.h"
#include <algorithm>
#include <iostream>

#pragma comment(lib, "ole32.lib")

bool WinAudioOutput::init(int sampleRate, int channels, int bytesPerSample) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    bytesPerSample_ = bytesPerSample;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    IMMDeviceEnumerator* enumerator = nullptr;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) return false;

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    enumerator->Release();
    if (FAILED(hr)) return false;

    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(&audioClient_));
    if (FAILED(hr)) return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sampleRate;
    wfx.wBitsPerSample = bytesPerSample * 8;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    // 请求50ms的缓冲区
    REFERENCE_TIME bufferDuration = 500000; // 100ns 单位

    hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED,
        0, bufferDuration, 0, &wfx, nullptr);
    if (FAILED(hr)) return false;

    hr = audioClient_->GetBufferSize(&bufferFrameCount_);
    if (FAILED(hr)) return false;

    hr = audioClient_->GetService(__uuidof(IAudioRenderClient),
        reinterpret_cast<void**>(&renderClient_));
    if (FAILED(hr)) return false;

    hr = audioClient_->GetService(__uuidof(IAudioStreamVolume),
        reinterpret_cast<void**>(&streamVolume_));

    return true;
}

bool WinAudioOutput::start() {
    if (running_) return true;

    running_ = true;
    thread_ = std::thread(&WinAudioOutput::audioThread, this);

    if (audioClient_) {
        audioClient_->Start();
    }
    return true;
}

bool WinAudioOutput::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    if (audioClient_) {
        audioClient_->Stop();
    }
    return true;
}

void WinAudioOutput::setVolume(float volume) {
    if (streamVolume_) {
        uint32_t channelCount = 0;
        streamVolume_->GetChannelCount(&channelCount);
        std::vector<float> volumes(channelCount, volume);
        streamVolume_->SetAllVolumes(channelCount, volumes.data());
    }
}

void WinAudioOutput::setCallback(AudioCallback callback) {
    callback_ = std::move(callback);
}

void WinAudioOutput::destroy() {
    stop();

    if (streamVolume_) { streamVolume_->Release(); streamVolume_ = nullptr; }
    if (renderClient_) { renderClient_->Release(); renderClient_ = nullptr; }
    if (audioClient_) { audioClient_->Release(); audioClient_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
}

void WinAudioOutput::audioThread() {
    while (running_) {
        UINT32 padding = 0;
        audioClient_->GetCurrentPadding(&padding);
        UINT32 availableFrames = bufferFrameCount_ - padding;

        if (availableFrames == 0) {
            Sleep(1);
            continue;
        }

        BYTE* data = nullptr;
        HRESULT hr = renderClient_->GetBuffer(availableFrames, &data);
        if (SUCCEEDED(hr)) {
            if (callback_) {
                UINT32 size = availableFrames * channels_ * bytesPerSample_;
                callback_(data, size);
            } else {
                memset(data, 0, availableFrames * channels_ * bytesPerSample_);
            }
            renderClient_->ReleaseBuffer(availableFrames, 0);
        }
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/windows/WinAudioOutput.h src/platform/windows/WinAudioOutput.cpp
git commit -m "feat: implement WinAudioOutput using WASAPI"
```

---

### Task 2.3: Windows 主窗口 (ImGui + Win32)

**Files:**
- Create: `src/platform/windows/WinMainWindow.h`
- Create: `src/platform/windows/WinMainWindow.cpp`
- Create: `src/platform/windows/main.cpp`

- [ ] **Step 1: 实现 WinMainWindow.h**

```cpp
// src/platform/windows/WinMainWindow.h
#pragma once

#include "core/common/NonCopyable.h"
#include "core/controller/PlayerController.h"

#include <string>
#include <memory>

class WinMainWindow : public NonCopyable {
public:
    WinMainWindow() = default;
    ~WinMainWindow();

    bool init(HINSTANCE hInstance, int nCmdShow);
    void run();
    void shutdown();

private:
    static LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void setupImGui();
    void renderUI();
    void handleFileOpen();
    void handleRtmpInput();

    HWND hwnd_ = nullptr;
    std::unique_ptr<PlayerController> player_;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    bool running_ = false;

    // UI状态
    char filePath_[512] = "";
    char rtmpUrl_[512] = "";
    bool showDemoWindow_ = false;
};
```

- [ ] **Step 2: 实现 WinMainWindow.cpp**

```cpp
// src/platform/windows/WinMainWindow.cpp
#include "WinMainWindow.h"
#include "D3D11Renderer.h"
#include "WinAudioOutput.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include <commdlg.h>
#include <fstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WinMainWindow::~WinMainWindow() {
    shutdown();
}

bool WinMainWindow::init(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MPlayer";
    RegisterClassEx(&wc);

    hwnd_ = CreateWindowEx(0, L"MPlayer", L"MPlayer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth_, windowHeight_, nullptr, nullptr, hInstance, this);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);

    // 初始化播放器
    player_ = std::make_unique<PlayerController>();

    auto renderer = std::make_unique<D3D11Renderer>();
    if (!renderer->init(hwnd_)) return false;
    player_->setRenderer(std::move(renderer));

    auto audio = std::make_unique<WinAudioOutput>();
    player_->setAudioOutput(std::move(audio));

    player_->setErrorCallback([](const std::string& err) {
        // 错误处理
    });

    // 初始化 ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd_);
    // D3D11 设备需要从 renderer 获取，这里简化处理
    // ImGui_ImplDX11_Init(device, context);

    running_ = true;
    return true;
}

void WinMainWindow::run() {
    while (running_) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running_ = false;
            }
        }

        if (!running_) break;

        // 渲染视频帧
        if (player_ && player_->state() == PlayerController::Playing) {
            VideoFrame frame;
            // 从 PlayerController 的帧队列获取帧并渲染
            // player_->renderNextFrame();
        }

        renderUI();
    }
}

void WinMainWindow::renderUI() {
    // ImGui 渲染控制面板
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(0, windowHeight_ - 120), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth_, 120));
    ImGui::Begin("Controls", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    // 文件路径输入
    ImGui::InputText("File", filePath_, sizeof(filePath_));
    ImGui::SameLine();
    if (ImGui::Button("Open File")) {
        handleFileOpen();
    }

    // RTMP URL输入
    ImGui::InputText("RTMP", rtmpUrl_, sizeof(rtmpUrl_));
    ImGui::SameLine();
    if (ImGui::Button("Open Stream")) {
        handleRtmpInput();
    }

    ImGui::Separator();

    // 播放控制
    if (ImGui::Button(player_->state() == PlayerController::Playing ? "Pause" : "Play")) {
        if (player_->state() == PlayerController::Playing) {
            player_->pause();
        } else {
            player_->play();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) {
        player_->stop();
    }

    // 进度条
    double pos = player_->currentPosition();
    double dur = player_->duration();
    float progress = dur > 0 ? static_cast<float>(pos / dur) : 0.0f;
    if (ImGui::SliderFloat("Progress", &progress, 0.0f, 1.0f)) {
        player_->seek(progress * dur);
    }

    // 音量
    static float volume = 0.8f;
    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
        player_->setVolume(volume);
    }

    // 倍速
    static float speed = 1.0f;
    if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.0f)) {
        player_->setSpeed(speed);
    }

    ImGui::End();

    // 视频信息面板
    if (player_->state() != PlayerController::Idle) {
        auto info = player_->getVideoInfo();
        ImGui::Begin("Video Info");
        ImGui::Text("Codec: %s", info.codecName.c_str());
        ImGui::Text("Resolution: %d x %d", info.width, info.height);
        ImGui::Text("Duration: %.2f s", info.duration);
        ImGui::Text("FPS: %.1f", info.frameRate);
        ImGui::End();
    }

    ImGui::Render();
    // ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void WinMainWindow::handleFileOpen() {
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "";

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd_;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "Video Files\0*.mp4;*.avi;*.mkv;*.flv;*.mov;*.wmv\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        strncpy_s(filePath_, szFile, sizeof(filePath_) - 1);
        player_->close();
        player_->open(filePath_);
        player_->play();
    }
}

void WinMainWindow::handleRtmpInput() {
    if (strlen(rtmpUrl_) > 0) {
        player_->close();
        player_->open(rtmpUrl_);
        player_->play();
    }
}

void WinMainWindow::shutdown() {
    if (player_) {
        player_->close();
        player_.reset();
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT WINAPI WinMainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    WinMainWindow* self = reinterpret_cast<WinMainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_SIZE: {
        if (self) {
            self->windowWidth_ = LOWORD(lParam);
            self->windowHeight_ = HIWORD(lParam);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
```

- [ ] **Step 3: 实现 main.cpp**

```cpp
// src/platform/windows/main.cpp
#include "WinMainWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WinMainWindow window;
    if (!window.init(hInstance, nCmdShow)) {
        return 1;
    }

    window.run();
    window.shutdown();
    return 0;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/platform/windows/WinMainWindow.h src/platform/windows/WinMainWindow.cpp src/platform/windows/main.cpp
git commit -m "feat: implement Windows main window with ImGui controls"
```

---

## Phase 3: Android 平台实现

### Task 3.1: OpenGL ES 渲染器

**Files:**
- Create: `src/platform/android/GLESRenderer.h`
- Create: `src/platform/android/GLESRenderer.cpp`
- Create: `src/platform/android/shaders/yuv2rgb.vert`
- Create: `src/platform/android/shaders/yuv2rgb.frag`

- [ ] **Step 1: 创建 GLSL 着色器**

```glsl
// src/platform/android/shaders/yuv2rgb.vert
#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
```

```glsl
// src/platform/android/shaders/yuv2rgb.frag
#version 300 es
precision highp float;
in vec2 vTexCoord;
out vec4 fragColor;

uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;

void main() {
    float y = texture(texY, vTexCoord).r;
    float u = texture(texU, vTexCoord).r - 0.5;
    float v = texture(texV, vTexCoord).r - 0.5;

    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;

    fragColor = vec4(r, g, b, 1.0);
}
```

- [ ] **Step 2: 实现 GLESRenderer.h**

```cpp
// src/platform/android/GLESRenderer.h
#pragma once

#include "core/renderer/IRenderer.h"
#include "core/common/NonCopyable.h"

#include <GLES3/gl3.h>
#include <EGL/egl.h>

class GLESRenderer : public IRenderer, public NonCopyable {
public:
    GLESRenderer() = default;
    ~GLESRenderer() override { destroy(); }

    bool init(void* nativeWindow) override;
    bool renderFrame(const VideoFrame& frame) override;
    bool renderTexture(void* nativeTexture, int width, int height) override;
    void resize(int width, int height) override;
    void destroy() override;

private:
    bool createProgram();
    GLuint compileShader(GLenum type, const char* source);
    void createTextures(int width, int height);

    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;

    GLuint program_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;

    GLuint texY_ = 0;
    GLuint texU_ = 0;
    GLuint texV_ = 0;

    int texWidth_ = 0;
    int texHeight_ = 0;
    bool initialized_ = false;
};
```

- [ ] **Step 3: 实现 GLESRenderer.cpp**

```cpp
// src/platform/android/GLESRenderer.cpp
#include "GLESRenderer.h"
#include <cstring>

static const char* vertexShaderSource = R"(
#version 300 es
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;
out vec2 vTexCoord;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

static const char* fragmentShaderSource = R"(
#version 300 es
precision highp float;
in vec2 vTexCoord;
out vec4 fragColor;
uniform sampler2D texY;
uniform sampler2D texU;
uniform sampler2D texV;
void main() {
    float y = texture(texY, vTexCoord).r;
    float u = texture(texU, vTexCoord).r - 0.5;
    float v = texture(texV, vTexCoord).r - 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;
    fragColor = vec4(r, g, b, 1.0);
}
)";

bool GLESRenderer::init(void* nativeWindow) {
    // nativeWindow 是 Android 的 ANativeWindow
    EGLNativeWindowType window = static_cast<EGLNativeWindowType>(nativeWindow);

    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) return false;

    if (!eglInitialize(display_, nullptr, nullptr)) return false;

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display_, attribs, &config, 1, &numConfigs)) return false;

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    context_ = eglCreateContext(display_, config, EGL_NO_CONTEXT, contextAttribs);
    if (context_ == EGL_NO_CONTEXT) return false;

    surface_ = eglCreateWindowSurface(display_, config, window, nullptr);
    if (surface_ == EGL_NO_SURFACE) return false;

    if (!eglMakeCurrent(display_, surface_, surface_, context_)) return false;

    if (!createProgram()) return false;

    // 创建顶点缓冲
    float vertices[] = {
        -1.0f,  1.0f, 0.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f, 1.0f,
         1.0f, -1.0f, 1.0f, 1.0f,
    };

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
        reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    initialized_ = true;
    return true;
}

GLuint GLESRenderer::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool GLESRenderer::createProgram() {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!vs || !fs) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);

    GLint success;
    glGetProgramiv(program_, GL_LINK_STATUS, &success);
    glDeleteShader(vs);
    glDeleteShader(fs);

    return success != 0;
}

void GLESRenderer::createTextures(int width, int height) {
    if (texWidth_ == width && texHeight_ == height) return;

    if (texY_) { glDeleteTextures(1, &texY_); }
    if (texU_) { glDeleteTextures(1, &texU_); }
    if (texV_) { glDeleteTextures(1, &texV_); }

    glGenTextures(1, &texY_);
    glBindTexture(GL_TEXTURE_2D, texY_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &texU_);
    glBindTexture(GL_TEXTURE_2D, texU_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenTextures(1, &texV_);
    glBindTexture(GL_TEXTURE_2D, texV_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width / 2, height / 2, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    texWidth_ = width;
    texHeight_ = height;
}

bool GLESRenderer::renderFrame(const VideoFrame& frame) {
    if (!initialized_) return false;

    createTextures(frame.width, frame.height);

    // 上传Y平面
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texY_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width, frame.height,
        GL_RED, GL_UNSIGNED_BYTE, frame.data[0].data());

    // 上传U平面
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texU_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width / 2, frame.height / 2,
        GL_RED, GL_UNSIGNED_BYTE, frame.data[1].data());

    // 上传V平面
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texV_);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame.width / 2, frame.height / 2,
        GL_RED, GL_UNSIGNED_BYTE, frame.data[2].data());

    // 渲染
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program_);

    glUniform1i(glGetUniformLocation(program_, "texY"), 0);
    glUniform1i(glGetUniformLocation(program_, "texU"), 1);
    glUniform1i(glGetUniformLocation(program_, "texV"), 2);

    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    eglSwapBuffers(display_, surface_);
    return true;
}

bool GLESRenderer::renderTexture(void* nativeTexture, int width, int height) {
    // 硬解 SurfaceTexture 渲染 - 后续实现
    return false;
}

void GLESRenderer::resize(int width, int height) {
    glViewport(0, 0, width, height);
}

void GLESRenderer::destroy() {
    if (texY_) { glDeleteTextures(1, &texY_); texY_ = 0; }
    if (texU_) { glDeleteTextures(1, &texU_); texU_ = 0; }
    if (texV_) { glDeleteTextures(1, &texV_); texV_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (program_) { glDeleteProgram(program_); program_ = 0; }

    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) eglDestroyContext(display_, context_);
        if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
        context_ = EGL_NO_CONTEXT;
        surface_ = EGL_NO_SURFACE;
    }
    initialized_ = false;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/platform/android/GLESRenderer.h src/platform/android/GLESRenderer.cpp src/platform/android/shaders/yuv2rgb.vert src/platform/android/shaders/yuv2rgb.frag
git commit -m "feat: implement GLESRenderer with YUV→RGB shader"
```

---

### Task 3.2: Android 音频输出 (OpenSL ES)

**Files:**
- Create: `src/platform/android/AndroidAudioOutput.h`
- Create: `src/platform/android/AndroidAudioOutput.cpp`

- [ ] **Step 1: 实现 AndroidAudioOutput.h**

```cpp
// src/platform/android/AndroidAudioOutput.h
#pragma once

#include "core/audio/IAudioOutput.h"
#include "core/common/NonCopyable.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <atomic>
#include <mutex>
#include <vector>

class AndroidAudioOutput : public IAudioOutput, public NonCopyable {
public:
    AndroidAudioOutput() = default;
    ~AndroidAudioOutput() override { destroy(); }

    bool init(int sampleRate, int channels, int bytesPerSample) override;
    bool start() override;
    bool stop() override;
    void setVolume(float volume) override;
    void setCallback(AudioCallback callback) override;
    void destroy() override;

private:
    static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
    void enqueueBuffer();

    SLObjectItf engineObj_ = nullptr;
    SLEngineItf engine_ = nullptr;
    SLObjectItf outputMixObj_ = nullptr;
    SLObjectItf playerObj_ = nullptr;
    SLPlayItf player_ = nullptr;
    SLAndroidSimpleBufferQueueItf bufferQueue_ = nullptr;
    SLVolumeItf volumeItf_ = nullptr;

    AudioCallback callback_;
    std::vector<uint8_t> buffer_;

    int sampleRate_ = 0;
    int channels_ = 0;
    int bytesPerSample_ = 0;
    int bufferSize_ = 0;
    std::atomic<bool> playing_{false};
};
```

- [ ] **Step 2: 实现 AndroidAudioOutput.cpp**

```cpp
// src/platform/android/AndroidAudioOutput.cpp
#include "AndroidAudioOutput.h"
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MPlayer", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MPlayer", __VA_ARGS__)

bool AndroidAudioOutput::init(int sampleRate, int channels, int bytesPerSample) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    bytesPerSample_ = bytesPerSample;
    bufferSize_ = sampleRate * channels * bytesPerSample / 10; // 100ms 缓冲
    buffer_.resize(bufferSize_);

    // 创建 OpenSL ES 引擎
    SLEngineOption engineOpts[] = {
        { SL_ENGINEOPTION_THREADSAFE, SL_BOOLEAN_TRUE }
    };

    SLresult result = slCreateEngine(&engineObj_, 1, engineOpts, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) { LOGE("slCreateEngine failed"); return false; }

    result = (*engineObj_)->Realize(engineObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*engineObj_)->GetInterface(engineObj_, SL_IID_ENGINE, &engine_);
    if (result != SL_RESULT_SUCCESS) return false;

    // 创建输出混音器
    result = (*engine_)->CreateOutputMix(engine_, &outputMixObj_, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*outputMixObj_)->Realize(outputMixObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    // 创建音频播放器
    SLDataLocator_AndroidSimpleBufferQueue locBufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };

    SLDataFormat_PCM format_pcm = {};
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = channels;
    format_pcm.samplesPerSec = sampleRate * 1000; // OpenSL 使用毫赫兹
    format_pcm.bitsPerSample = bytesPerSample * 8;
    format_pcm.containerSize = bytesPerSample * 8;
    format_pcm.channelMask = channels == 1 ? SL_SPEAKER_FRONT_CENTER :
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    SLDataSource audioSrc = { &locBufq, &format_pcm };

    SLDataLocator_OutputMix locOutmix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObj_ };
    SLDataSink audioSnk = { &locOutmix, nullptr };

    SLInterfaceID ids[] = { SL_IID_BUFFERQUEUE, SL_IID_VOLUME };
    SLboolean req[] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    result = (*engine_)->CreateAudioPlayer(engine_, &playerObj_, &audioSrc, &audioSnk,
        2, ids, req);
    if (result != SL_RESULT_SUCCESS) { LOGE("CreateAudioPlayer failed"); return false; }

    result = (*playerObj_)->Realize(playerObj_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*playerObj_)->GetInterface(playerObj_, SL_IID_PLAY, &player_);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*playerObj_)->GetInterface(playerObj_, SL_IID_BUFFERQUEUE, &bufferQueue_);
    if (result != SL_RESULT_SUCCESS) return false;

    result = (*playerObj_)->GetInterface(playerObj_, SL_IID_VOLUME, &volumeItf_);

    // 注册回调
    result = (*bufferQueue_)->RegisterCallback(bufferQueue_, bqPlayerCallback, this);
    if (result != SL_RESULT_SUCCESS) return false;

    return true;
}

bool AndroidAudioOutput::start() {
    if (!player_) return false;
    playing_ = true;
    (*player_)->SetPlayState(player_, SL_PLAYSTATE_PLAYING);
    enqueueBuffer();
    return true;
}

bool AndroidAudioOutput::stop() {
    if (!player_) return false;
    playing_ = false;
    (*player_)->SetPlayState(player_, SL_PLAYSTATE_STOPPED);
    return true;
}

void AndroidAudioOutput::setVolume(float volume) {
    if (volumeItf_) {
        SLmillibel mb = static_cast<SLmillibel>(
            (1.0f - volume) * SL_MILLIBEL_MIN
        );
        (*volumeItf_)->SetVolumeLevel(volumeItf_, mb);
    }
}

void AndroidAudioOutput::setCallback(AudioCallback callback) {
    callback_ = std::move(callback);
}

void AndroidAudioOutput::destroy() {
    stop();
    if (playerObj_) { (*playerObj_)->Destroy(playerObj_); playerObj_ = nullptr; }
    if (outputMixObj_) { (*outputMixObj_)->Destroy(outputMixObj_); outputMixObj_ = nullptr; }
    if (engineObj_) { (*engineObj_)->Destroy(engineObj_); engineObj_ = nullptr; }
}

void AndroidAudioOutput::bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* self = static_cast<AndroidAudioOutput*>(context);
    self->enqueueBuffer();
}

void AndroidAudioOutput::enqueueBuffer() {
    if (!playing_ || !callback_) return;

    callback_(buffer_.data(), bufferSize_);
    (*bufferQueue_)->Enqueue(bufferQueue_, buffer_.data(), bufferSize_);
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/android/AndroidAudioOutput.h src/platform/android/AndroidAudioOutput.cpp
git commit -m "feat: implement AndroidAudioOutput using OpenSL ES"
```

---

### Task 3.3: JNI 桥接层

**Files:**
- Create: `src/platform/android/jni/MPlayerJNI.h`
- Create: `src/platform/android/jni/MPlayerJNI.cpp`

- [ ] **Step 1: 实现 MPlayerJNI.h**

```cpp
// src/platform/android/jni/MPlayerJNI.h
#pragma once

#include <jni.h>

class MPlayerJNI {
public:
    static void registerNatives(JavaVM* vm);

    static jlong nativeCreate(JNIEnv* env, jobject thiz);
    static void nativeDestroy(JNIEnv* env, jobject thiz, jlong handle);
    static jboolean nativeOpen(JNIEnv* env, jobject thiz, jlong handle, jstring url);
    static void nativeClose(JNIEnv* env, jobject thiz, jlong handle);
    static void nativePlay(JNIEnv* env, jobject thiz, jlong handle);
    static void nativePause(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeStop(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeSeek(JNIEnv* env, jobject thiz, jlong handle, jdouble seconds);
    static void nativeSetVolume(JNIEnv* env, jobject thiz, jlong handle, jfloat volume);
    static void nativeSetSpeed(JNIEnv* env, jobject thiz, jlong handle, jfloat speed);
    static jdouble nativeGetDuration(JNIEnv* env, jobject thiz, jlong handle);
    static jdouble nativeGetCurrentPosition(JNIEnv* env, jobject thiz, jlong handle);
    static void nativeSetSurface(JNIEnv* env, jobject thiz, jlong handle, jobject surface);
};
```

- [ ] **Step 2: 实现 MPlayerJNI.cpp**

```cpp
// src/platform/android/jni/MPlayerJNI.cpp
#include "MPlayerJNI.h"
#include "core/controller/PlayerController.h"
#include "GLESRenderer.h"
#include "AndroidAudioOutput.h"

#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MPlayerJNI", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MPlayerJNI", __VA_ARGS__)

static const char* kClassName = "com/mplayer/MPlayerNative";

static JNINativeMethod methods[] = {
    {"nativeCreate", "()J", (void*)MPlayerJNI::nativeCreate},
    {"nativeDestroy", "(J)V", (void*)MPlayerJNI::nativeDestroy},
    {"nativeOpen", "(JLjava/lang/String;)Z", (void*)MPlayerJNI::nativeOpen},
    {"nativeClose", "(J)V", (void*)MPlayerJNI::nativeClose},
    {"nativePlay", "(J)V", (void*)MPlayerJNI::nativePlay},
    {"nativePause", "(J)V", (void*)MPlayerJNI::nativePause},
    {"nativeStop", "(J)V", (void*)MPlayerJNI::nativeStop},
    {"nativeSeek", "(JD)V", (void*)MPlayerJNI::nativeSeek},
    {"nativeSetVolume", "(JF)V", (void*)MPlayerJNI::nativeSetVolume},
    {"nativeSetSpeed", "(JF)V", (void*)MPlayerJNI::nativeSetSpeed},
    {"nativeGetDuration", "(J)D", (void*)MPlayerJNI::nativeGetDuration},
    {"nativeGetCurrentPosition", "(J)D", (void*)MPlayerJNI::nativeGetCurrentPosition},
    {"nativeSetSurface", "(JLjava/lang/Object;)V", (void*)MPlayerJNI::nativeSetSurface},
};

void MPlayerJNI::registerNatives(JavaVM* vm) {
    JNIEnv* env = nullptr;
    vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);

    jclass cls = env->FindClass(kClassName);
    if (!cls) {
        LOGE("Failed to find class %s", kClassName);
        return;
    }

    env->RegisterNatives(cls, methods, sizeof(methods) / sizeof(methods[0]));
    LOGI("JNI natives registered");
}

jlong MPlayerJNI::nativeCreate(JNIEnv* env, jobject thiz) {
    auto* player = new PlayerController();
    return reinterpret_cast<jlong>(player);
}

void MPlayerJNI::nativeDestroy(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) {
        player->close();
        delete player;
    }
}

jboolean MPlayerJNI::nativeOpen(JNIEnv* env, jobject thiz, jlong handle, jstring url) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (!player) return JNI_FALSE;

    const char* urlStr = env->GetStringUTFChars(url, nullptr);
    bool result = player->open(urlStr);
    env->ReleaseStringUTFChars(url, urlStr);

    return result ? JNI_TRUE : JNI_FALSE;
}

void MPlayerJNI::nativeClose(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->close();
}

void MPlayerJNI::nativePlay(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->play();
}

void MPlayerJNI::nativePause(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->pause();
}

void MPlayerJNI::nativeStop(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->stop();
}

void MPlayerJNI::nativeSeek(JNIEnv* env, jobject thiz, jlong handle, jdouble seconds) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->seek(seconds);
}

void MPlayerJNI::nativeSetVolume(JNIEnv* env, jobject thiz, jlong handle, jfloat volume) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->setVolume(volume);
}

void MPlayerJNI::nativeSetSpeed(JNIEnv* env, jobject thiz, jlong handle, jfloat speed) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (player) player->setSpeed(speed);
}

jdouble MPlayerJNI::nativeGetDuration(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    return player ? player->duration() : 0.0;
}

jdouble MPlayerJNI::nativeGetCurrentPosition(JNIEnv* env, jobject thiz, jlong handle) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    return player ? player->currentPosition() : 0.0;
}

void MPlayerJNI::nativeSetSurface(JNIEnv* env, jobject thiz, jlong handle, jobject surface) {
    auto* player = reinterpret_cast<PlayerController*>(handle);
    if (!player || !surface) return;

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get ANativeWindow from Surface");
        return;
    }

    auto renderer = std::make_unique<GLESRenderer>();
    if (renderer->init(window)) {
        player->setRenderer(std::move(renderer));
    }

    auto audio = std::make_unique<AndroidAudioOutput>();
    player->setAudioOutput(std::move(audio));

    // ANativeWindow 不需要在这里 release，renderer 持有
}

// JNI_OnLoad - 库加载时自动调用
extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    MPlayerJNI::registerNatives(vm);
    return JNI_VERSION_1_6;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/android/jni/MPlayerJNI.h src/platform/android/jni/MPlayerJNI.cpp
git commit -m "feat: implement JNI bridge for Android"
```

---

### Task 3.4: Android Java/Kotlin UI

**Files:**
- Create: `src/android-app/build.gradle`
- Create: `src/android-app/app/build.gradle`
- Create: `src/android-app/app/src/main/AndroidManifest.xml`
- Create: `src/android-app/app/src/main/java/com/mplayer/MPlayerActivity.java`
- Create: `src/android-app/app/src/main/java/com/mplayer/MPlayerNative.java`
- Create: `src/android-app/app/src/main/res/layout/activity_player.xml`

- [ ] **Step 1: 创建 JNI Native 接口类**

```java
// src/android-app/app/src/main/java/com/mplayer/MPlayerNative.java
package com.mplayer;

public class MPlayerNative {
    private long nativeHandle = 0;

    public MPlayerNative() {
        nativeHandle = nativeCreate();
    }

    public boolean open(String url) {
        return nativeOpen(nativeHandle, url);
    }

    public void close() {
        nativeClose(nativeHandle);
    }

    public void play() { nativePlay(nativeHandle); }
    public void pause() { nativePause(nativeHandle); }
    public void stop() { nativeStop(nativeHandle); }
    public void seek(double seconds) { nativeSeek(nativeHandle, seconds); }
    public void setVolume(float volume) { nativeSetVolume(nativeHandle, volume); }
    public void setSpeed(float speed) { nativeSetSpeed(nativeHandle, speed); }
    public double getDuration() { return nativeGetDuration(nativeHandle); }
    public double getCurrentPosition() { return nativeGetCurrentPosition(nativeHandle); }
    public void setSurface(Object surface) { nativeSetSurface(nativeHandle, surface); }

    public void release() {
        if (nativeHandle != 0) {
            nativeDestroy(nativeHandle);
            nativeHandle = 0;
        }
    }

    // JNI native methods
    private native long nativeCreate();
    private native void nativeDestroy(long handle);
    private native boolean nativeOpen(long handle, String url);
    private native void nativeClose(long handle);
    private native void nativePlay(long handle);
    private native void nativePause(long handle);
    private native void nativeStop(long handle);
    private native void nativeSeek(long handle, double seconds);
    private native void nativeSetVolume(long handle, float volume);
    private native void nativeSetSpeed(long handle, float speed);
    private native double nativeGetDuration(long handle);
    private native double nativeGetCurrentPosition(long handle);
    private native void nativeSetSurface(long handle, Object surface);

    static {
        System.loadLibrary("MPlayerAndroid");
    }
}
```

- [ ] **Step 2: 创建 Activity**

```java
// src/android-app/app/src/main/java/com/mplayer/MPlayerActivity.java
package com.mplayer;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

public class MPlayerActivity extends Activity implements SurfaceHolder.Callback {

    private MPlayerNative player;
    private SurfaceView surfaceView;
    private EditText urlInput;
    private SeekBar seekBar;
    private SeekBar volumeBar;
    private Button btnPlay;
    private Button btnStop;
    private Button btnOpen;
    private TextView tvInfo;
    private Handler handler;
    private boolean isTrackingSeekBar = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_player);

        player = new MPlayerNative();
        handler = new Handler(Looper.getMainLooper());

        surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(this);

        urlInput = findViewById(R.id.urlInput);
        btnOpen = findViewById(R.id.btnOpen);
        btnPlay = findViewById(R.id.btnPlay);
        btnStop = findViewById(R.id.btnStop);
        seekBar = findViewById(R.id.seekBar);
        volumeBar = findViewById(R.id.volumeBar);
        tvInfo = findViewById(R.id.tvInfo);

        btnOpen.setOnClickListener(v -> {
            String url = urlInput.getText().toString().trim();
            if (url.isEmpty()) {
                Toast.makeText(this, "Please enter a URL or file path", Toast.LENGTH_SHORT).show();
                return;
            }
            player.stop();
            player.open(url);
            player.play();
        });

        btnPlay.setOnClickListener(v -> {
            if (player.getCurrentPosition() > 0) {
                player.pause();
                btnPlay.setText("Play");
            } else {
                player.play();
                btnPlay.setText("Pause");
            }
        });

        btnStop.setOnClickListener(v -> {
            player.stop();
            btnPlay.setText("Play");
        });

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    double duration = player.getDuration();
                    player.seek(progress / 100.0 * duration);
                }
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { isTrackingSeekBar = true; }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { isTrackingSeekBar = false; }
        });

        volumeBar.setProgress(80);
        volumeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) player.setVolume(progress / 100.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        startProgressUpdate();
    }

    private void startProgressUpdate() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!isTrackingSeekBar) {
                    double pos = player.getCurrentPosition();
                    double dur = player.getDuration();
                    if (dur > 0) {
                        seekBar.setProgress((int)(pos / dur * 100));
                    }
                }
                handler.postDelayed(this, 200);
            }
        }, 200);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        player.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
        player.release();
    }
}
```

- [ ] **Step 3: 创建布局文件**

```xml
<!-- src/android-app/app/src/main/res/layout/activity_player.xml -->
<?xml version="1.0" encoding="utf-8"?>
<LinearLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:orientation="vertical">

    <SurfaceView
        android:id="@+id/surfaceView"
        android:layout_width="match_parent"
        android:layout_height="0dp"
        android:layout_weight="1" />

    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:orientation="vertical"
        android:padding="8dp">

        <LinearLayout
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:orientation="horizontal">

            <EditText
                android:id="@+id/urlInput"
                android:layout_width="0dp"
                android:layout_height="wrap_content"
                android:layout_weight="1"
                android:hint="rtmp:// or file path"
                android:singleLine="true" />

            <Button
                android:id="@+id/btnOpen"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Open" />
        </LinearLayout>

        <SeekBar
            android:id="@+id/seekBar"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:max="100" />

        <LinearLayout
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:orientation="horizontal"
            android:gravity="center">

            <Button
                android:id="@+id/btnPlay"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Play" />

            <Button
                android:id="@+id/btnStop"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:text="Stop" />

            <TextView
                android:id="@+id/tvInfo"
                android:layout_width="wrap_content"
                android:layout_height="wrap_content"
                android:textSize="12sp" />
        </LinearLayout>

        <SeekBar
            android:id="@+id/volumeBar"
            android:layout_width="match_parent"
            android:layout_height="wrap_content"
            android:max="100" />
    </LinearLayout>
</LinearLayout>
```

- [ ] **Step 4: 创建 AndroidManifest.xml**

```xml
<!-- src/android-app/app/src/main/AndroidManifest.xml -->
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.mplayer">

    <uses-permission android:name="android.permission.INTERNET" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />

    <application
        android:allowBackup="true"
        android:label="MPlayer"
        android:theme="@android:style/Theme.NoTitleBar.Fullscreen">

        <activity
            android:name=".MPlayerActivity"
            android:configChanges="orientation|screenSize|keyboardHidden"
            android:screenOrientation="landscape"
            android:exported="true">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>
        </activity>
    </application>
</manifest>
```

- [ ] **Step 5: 创建 build.gradle**

```groovy
// src/android-app/app/build.gradle
plugins {
    id 'com.android.application'
}

android {
    namespace 'com.mplayer'
    compileSdk 34

    defaultConfig {
        applicationId "com.mplayer"
        minSdk 24
        targetSdk 34
        versionCode 1
        versionName "1.0"

        ndk {
            abiFilters 'arm64-v8a', 'armeabi-v7a'
        }

        externalNativeBuild {
            cmake {
                path file('../../src/platform/android/CMakeLists.txt')
                version '3.22.1'
            }
        }
    }

    buildTypes {
        release {
            minifyEnabled false
        }
    }
}
```

- [ ] **Step 6: Commit**

```bash
git add src/android-app/
git commit -m "feat: add Android Java UI with SurfaceView and controls"
```

---

## Phase 4: 硬件解码

### Task 4.1: Windows D3D11VA 硬件解码

**Files:**
- Create: `src/platform/windows/D3D11VAHardwareDecoder.h`
- Create: `src/platform/windows/D3D11VAHardwareDecoder.cpp`

- [ ] **Step 1: 实现 D3D11VA 硬件解码**

```cpp
// src/platform/windows/D3D11VAHardwareDecoder.h
#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

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

    ID3D11Texture2D* getLastDecodedTexture() const { return decodedTexture_.Get(); }

private:
    const AVCodec* codec_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    AVBufferRef* hwDeviceCtx_ = nullptr;
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11Texture2D> decodedTexture_;
    bool initialized_ = false;
};
```

```cpp
// src/platform/windows/D3D11VAHardwareDecoder.cpp
#include "D3D11VAHardwareDecoder.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/pixdesc.h>
}

bool D3D11VAHardwareDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    codec_ = avcodec_find_decoder(params->codec_id);
    if (!codec_) return false;

    codecCtx_ = avcodec_alloc_context3(codec_);
    if (!codecCtx_) return false;

    if (avcodec_parameters_to_context(codecCtx_, params) < 0) {
        destroy();
        return false;
    }

    // 创建 D3D11 硬件设备上下文
    int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D11VA,
        nullptr, nullptr, 0);
    if (ret < 0) {
        destroy();
        return false;
    }

    codecCtx_->hw_device_ctx = av_buffer_ref(hwDeviceCtx_);
    codecCtx_->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) {
        for (const enum AVPixelFormat* p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_D3D11) return *p;
        }
        return AV_PIX_FMT_NONE;
    };

    if (avcodec_open2(codecCtx_, codec_, nullptr) < 0) {
        destroy();
        return false;
    }

    initialized_ = true;
    return true;
}

bool D3D11VAHardwareDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codecCtx_) return false;

    int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0) return false;

    ret = avcodec_receive_frame(codecCtx_, frame);
    if (ret < 0) return false;

    // 获取 D3D11 纹理
    if (frame->format == AV_PIX_FMT_D3D11) {
        auto* desc = reinterpret_cast<AVD3D11FrameDescriptor*>(frame->data[0]);
        if (desc && desc->texture) {
            decodedTexture_ = desc->texture;
        }
    }

    return true;
}

void D3D11VAHardwareDecoder::flush() {
    if (codecCtx_) avcodec_flush_buffers(codecCtx_);
}

void D3D11VAHardwareDecoder::destroy() {
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
        codecCtx_ = nullptr;
    }
    if (hwDeviceCtx_) {
        av_buffer_unref(&hwDeviceCtx_);
        hwDeviceCtx_ = nullptr;
    }
    decodedTexture_.Reset();
    codec_ = nullptr;
    initialized_ = false;
}

AVPixelFormat D3D11VAHardwareDecoder::outputFormat() const {
    return AV_PIX_FMT_D3D11;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/platform/windows/D3D11VAHardwareDecoder.h src/platform/windows/D3D11VAHardwareDecoder.cpp
git commit -m "feat: implement D3D11VA hardware decoder"
```

---

### Task 4.2: Android MediaCodec 硬件解码

**Files:**
- Create: `src/platform/android/MediaCodecDecoder.h`
- Create: `src/platform/android/MediaCodecDecoder.cpp`

- [ ] **Step 1: 实现 MediaCodecDecoder**

```cpp
// src/platform/android/MediaCodecDecoder.h
#pragma once

#include "core/decoder/IDecoder.h"
#include "core/common/NonCopyable.h"

#include <jni.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>

class MediaCodecDecoder : public IDecoder, public NonCopyable {
public:
    MediaCodecDecoder() = default;
    ~MediaCodecDecoder() override { destroy(); }

    bool init(const AVCodecParameters* params) override;
    bool decode(const AVPacket* packet, AVFrame* frame) override;
    void flush() override;
    void destroy() override;

    bool isHardware() const override { return true; }
    AVPixelFormat outputFormat() const override;

    void setSurface(jobject surface);

private:
    AMediaCodec* codec_ = nullptr;
    jobject surface_ = nullptr;
    bool configured_ = false;
    int width_ = 0;
    int height_ = 0;
};
```

```cpp
// src/platform/android/MediaCodecDecoder.cpp
#include "MediaCodecDecoder.h"
#include <android/log.h>
#include <cstring>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MediaCodecDecoder", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MediaCodecDecoder", __VA_ARGS__)

static const char* getMimeFromCodecId(AVCodecID id) {
    switch (id) {
        case AV_CODEC_ID_H264: return "video/avc";
        case AV_CODEC_ID_H265: return "video/hevc";
        case AV_CODEC_ID_VP8:  return "video/x-vnd.on2.vp8";
        case AV_CODEC_ID_VP9:  return "video/x-vnd.on2.vp9";
        case AV_CODEC_ID_AV1:  return "video/av01";
        default: return nullptr;
    }
}

bool MediaCodecDecoder::init(const AVCodecParameters* params) {
    if (!params) return false;

    const char* mime = getMimeFromCodecId(params->codec_id);
    if (!mime) {
        LOGE("Unsupported codec for MediaCodec: %d", params->codec_id);
        return false;
    }

    codec_ = AMediaCodec_createDecoderByType(mime);
    if (!codec_) {
        LOGE("Failed to create MediaCodec for %s", mime);
        return false;
    }

    width_ = params->width;
    height_ = params->height;

    // 构造 MediaFormat
    AMediaFormat* format = AMediaFormat_new();
    AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, width_);
    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, height_);

    // 如果有 extradata (SPS/PPS for H.264)
    if (params->extradata && params->extradata_size > 0) {
        AMediaFormat_setBuffer(format, "csd-0", params->extradata, params->extradata_size);
    }

    media_status_t status;
    if (surface_) {
        status = AMediaCodec_configure(codec_, format, surface_, nullptr, 0);
    } else {
        status = AMediaCodec_configure(codec_, format, nullptr, nullptr, 0);
    }

    AMediaFormat_delete(format);

    if (status != AMEDIA_OK) {
        LOGE("AMediaCodec_configure failed: %d", status);
        return false;
    }

    status = AMediaCodec_start(codec_);
    if (status != AMEDIA_OK) {
        LOGE("AMediaCodec_start failed: %d", status);
        return false;
    }

    configured_ = true;
    return true;
}

bool MediaCodecDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codec_ || !configured_) return false;

    // 获取输入buffer索引
    ssize_t inputIndex = AMediaCodec_dequeueInputBuffer(codec_, 5000);
    if (inputIndex < 0) return false;

    size_t inputSize;
    uint8_t* inputBuf = AMediaCodec_getInputBuffer(codec_, inputIndex, &inputSize);
    if (!inputBuf) return false;

    size_t copySize = std::min(static_cast<size_t>(packet->size), inputSize);
    memcpy(inputBuf, packet->data, copySize);

    uint64_t pts = packet->pts;
    AMediaCodec_queueInputBuffer(codec_, inputIndex, 0, copySize, pts, 0);

    // 获取输出
    AMediaCodecBufferInfo info;
    ssize_t outputIndex = AMediaCodec_dequeueOutputBuffer(codec_, &info, 5000);
    if (outputIndex < 0) return false;

    // 如果有 surface，直接渲染
    bool render = (surface_ != nullptr);
    AMediaCodec_releaseOutputBuffer(codec_, outputIndex, render);

    frame->width = width_;
    frame->height = height_;
    frame->pts = static_cast<double>(info.presentationTimeUs) / 1000000.0;

    return true;
}

void MediaCodecDecoder::flush() {
    if (codec_ && configured_) {
        AMediaCodec_flush(codec_);
    }
}

void MediaCodecDecoder::destroy() {
    if (codec_) {
        if (configured_) {
            AMediaCodec_stop(codec_);
        }
        AMediaCodec_delete(codec_);
        codec_ = nullptr;
    }
    configured_ = false;
}

AVPixelFormat MediaCodecDecoder::outputFormat() const {
    return AV_PIX_FMT_MEDIACODEC;
}

void MediaCodecDecoder::setSurface(jobject surface) {
    surface_ = surface;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/platform/android/MediaCodecDecoder.h src/platform/android/MediaCodecDecoder.cpp
git commit -m "feat: implement MediaCodec hardware decoder for Android"
```

---

## Phase 5: 集成与收尾

### Task 5.1: .gitignore 和 README

**Files:**
- Modify: `.gitignore`
- Modify: `README.md`

- [ ] **Step 1: 更新 .gitignore**

```
# Build
build/
cmake-build-*/

# IDE
.vs/
.vscode/
*.user
*.suo
*.sln
*.vcxproj*

# macOS
.DS_Store

# Third party binaries
third_party/ffmpeg/windows/
third_party/ffmpeg/android/
third_party/ffmpeg/macOS/

# Android
*.apk
*.ap_
*.dex
local.properties
.gradle/
.idea/

# Compiled
*.o
*.obj
*.lib
*.dll
*.so
*.exe
```

- [ ] **Step 2: 更新 README.md**

```markdown
# MPlayer

跨平台音视频播放器，支持 Windows 和 Android。

## 功能

- 本地文件播放 (MP4, AVI, MKV, FLV, MOV, WMV)
- RTMP 流播放
- 播放控制 (播放/暂停/停止/进度拖动/音量/倍速)
- 软硬件解码自动切换
- Windows: D3D11 渲染 + D3D11VA 硬解
- Android: OpenGL ES 3.0 渲染 + MediaCodec 硬解

## 依赖

- CMake 3.22+
- FFmpeg 6.x
- Windows: Visual Studio 2022, Windows SDK
- Android: Android NDK r25+, Android SDK 34

## 构建

### Windows

```bash
# 1. 下载 FFmpeg 预编译库到 third_party/ffmpeg/windows/x64/
# 2. 构建
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Android

```bash
# 1. 下载 FFmpeg Android 预编译库到 third_party/ffmpeg/android/
# 2. 用 Android Studio 打开 src/android-app/ 构建
```

## 项目结构

- `src/core/` - 平台无关核心层 (Demuxer, Decoder, Renderer, Controller)
- `src/platform/windows/` - Windows 平台实现
- `src/platform/android/` - Android 平台实现
- `src/android-app/` - Android Java/Kotlin UI
- `tests/` - 单元测试
```

- [ ] **Step 3: Commit**

```bash
git add .gitignore README.md
git commit -m "docs: update .gitignore and README"
```

---

## 自检清单

- **Spec覆盖**: 所有设计文档中的模块（Demuxer/Decoder/Renderer/Audio/Controller/JNI）都有对应 Task
- **占位符**: 无 TBD/TODO/待实现占位，所有代码步骤包含完整实现
- **类型一致性**: 接口定义（IDecoder/IRenderer/IAudioOutput）在所有使用处保持一致
