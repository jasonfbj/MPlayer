# MPlayer RTMP完善 / 硬解渲染 / 高级功能 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 RTMP 自动重连、Windows/Android 硬件解码纹理渲染打通、音视频同步变速和 PNG 截图三大功能模块。

**Architecture:** 三层架构不变（Core → Platform Abstraction → Platform Implementation）。RTMP 重连封装在 Demuxer 内部透明处理；硬解渲染通过共享设备（Windows）和直接 Surface 输出（Android）实现；倍速通过 FFmpeg atempo 滤镜变速音频；截图通过 sws_scale 转 RGBA + stb_image_write 保存 PNG。

**Tech Stack:** C++17, FFmpeg 6.x (libavfilter atempo), D3D11, OpenGL ES 3.0, Android NDK MediaCodec, stb_image_write

---

## 文件结构

| 操作 | 文件路径 | 职责 |
|------|---------|------|
| 新建 | `src/core/demuxer/NetworkConfig.h` | 网络配置和连接状态定义 |
| 修改 | `src/core/demuxer/Demuxer.h` | 新增重连成员和方法 |
| 修改 | `src/core/demuxer/Demuxer.cpp` | 实现重连逻辑 |
| 修改 | `src/core/common/VideoFrame.h` | 新增 NativeTexture 结构体 |
| 修改 | `src/core/renderer/IRenderer.h` | renderTexture 参数改为 NativeTexture |
| 修改 | `src/core/decoder/DecoderFactory.h` | 新增 sharedDevice 参数 |
| 修改 | `src/core/decoder/DecoderFactory.cpp` | 传递 sharedDevice |
| 修改 | `src/core/controller/PlayerController.h` | 新增多项接口 |
| 修改 | `src/core/controller/PlayerController.cpp` | 实现三大功能 |
| 修改 | `src/platform/windows/D3D11Renderer.h` | 暴露设备，NV12着色器 |
| 修改 | `src/platform/windows/D3D11Renderer.cpp` | 实现 renderTexture |
| 修改 | `src/platform/windows/D3D11VAHardwareDecoder.h` | 共享设备支持 |
| 修改 | `src/platform/windows/D3D11VAHardwareDecoder.cpp` | 使用共享设备 |
| 修改 | `src/platform/windows/CMakeLists.txt` | 补充 D3D11VAHardwareDecoder |
| 修改 | `src/platform/windows/WinMainWindow.cpp` | 注册硬解、传递设备 |
| 修改 | `src/platform/android/GLESRenderer.h` | 更新 renderTexture 签名 |
| 修改 | `src/platform/android/GLESRenderer.cpp` | 实现 renderTexture |
| 修改 | `src/platform/android/MediaCodecDecoder.h` | 新增 JNI env 管理 |
| 修改 | `src/platform/android/MediaCodecDecoder.cpp` | Surface 输出完善 |
| 修改 | `src/platform/android/CMakeLists.txt` | 补充 MediaCodec + mediandk |
| 修改 | `src/platform/android/jni/MPlayerJNI.cpp` | 注册硬解、传递 Surface |
| 新建 | `src/core/audio/AudioResampler.h` | atempo 滤镜封装 |
| 新建 | `src/core/audio/AudioResampler.cpp` | 滤镜链实现 |
| 新建 | `third_party/stb/stb_image_write.h` | PNG 编码单头库 |
| 修改 | `src/core/CMakeLists.txt` | 新增 AudioResampler |

---

## Phase 1: RTMP 流完善（自动重连）

### Task 1.1: 创建 NetworkConfig

**Files:**
- Create: `src/core/demuxer/NetworkConfig.h`

- [ ] **Step 1: 创建 NetworkConfig.h**

```cpp
// src/core/demuxer/NetworkConfig.h
#pragma once

#include <functional>
#include <string>

struct NetworkConfig {
    int timeoutUs = 5000000;        // 网络超时 (微秒), 默认 5 秒
    int bufferSize = 1024000;       // 缓冲区大小 (字节)
    int maxDelay = 500000;          // 最大延迟 (微秒)
    int maxRetries = 3;             // 最大重连次数
    int retryBaseDelayMs = 1000;    // 重连基础间隔 (毫秒)
};

enum class ConnectionState {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Failed
};

using ConnectionCallback = std::function<void(ConnectionState, const std::string&)>;
```

- [ ] **Step 2: Commit**

```bash
git add src/core/demuxer/NetworkConfig.h
git commit -m "feat(rtmp): add NetworkConfig and ConnectionState definitions"
```

---

### Task 1.2: Demuxer 添加重连能力

**Files:**
- Modify: `src/core/demuxer/Demuxer.h`
- Modify: `src/core/demuxer/Demuxer.cpp`

- [ ] **Step 1: 修改 Demuxer.h — 新增成员和方法**

在 `#include <atomic>` 之后添加:
```cpp
#include "core/demuxer/NetworkConfig.h"
```

在 `private:` 区域之前新增公共方法:
```cpp
    // 带网络配置的打开（兼容原 open(url)）
    bool open(const std::string& url, const NetworkConfig& config);

    void setConnectionCallback(ConnectionCallback cb) { connCb_ = std::move(cb); }
    ConnectionState connectionState() const { return connState_; }
```

在 `private:` 区域新增成员:
```cpp
    NetworkConfig netConfig_;
    ConnectionCallback connCb_;
    std::atomic<ConnectionState> connState_{ConnectionState::Disconnected};

    void setConnState(ConnectionState s, const std::string& msg = "") {
        connState_ = s;
        if (connCb_) connCb_(s, msg);
    }
    bool reopenInternal();
```

- [ ] **Step 2: 修改 Demuxer.cpp — 实现重连逻辑**

将现有的 `bool Demuxer::open(const std::string& url)` 改为调用新重载。完整替换 `Demuxer.cpp`:

```cpp
#include "core/demuxer/Demuxer.h"
#include <cstring>
#include <algorithm>

Demuxer::Demuxer() = default;

Demuxer::~Demuxer() {
    close();
}

bool Demuxer::open(const std::string& url) {
    NetworkConfig config;
    // isLive 由 URL scheme 推断
    return open(url, config);
}

bool Demuxer::open(const std::string& url, const NetworkConfig& config) {
    if (opened_) close();

    netConfig_ = config;
    formatCtx_ = avformat_alloc_context();
    if (!formatCtx_) return false;

    currentUrl_ = url;
    setConnState(ConnectionState::Connecting, url);

    // 网络流设置
    if (url.find("rtmp://") == 0 || url.find("http://") == 0 || url.find("rtsp://") == 0) {
        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "timeout", std::to_string(netConfig_.timeoutUs).c_str(), 0);
        av_dict_set(&opts, "buffer_size", std::to_string(netConfig_.bufferSize).c_str(), 0);
        av_dict_set(&opts, "max_delay", std::to_string(netConfig_.maxDelay).c_str(), 0);
        if (url.find("rtmp://") == 0) {
            av_dict_set(&opts, "rtmp_live", "live", 0);
        }
        av_dict_set(&opts, "fflags", "nobuffer", 0);
        av_dict_set(&opts, "analyzeduration", std::to_string(netConfig_.timeoutUs).c_str(), 0);

        int ret = avformat_open_input(&formatCtx_, url.c_str(), nullptr, &opts);
        av_dict_free(&opts);
        if (ret < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            setConnState(ConnectionState::Failed, "Failed to open: " + url);
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
    setConnState(ConnectionState::Connected, url);
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
    setConnState(ConnectionState::Disconnected, "");
}

bool Demuxer::readPacket(AVPacket* packet) {
    if (!opened_ || eof_) return false;

    int ret = av_read_frame(formatCtx_, packet);
    if (ret < 0) {
        if (ret == AVERROR_EOF || avio_feof(formatCtx_->pb)) {
            eof_ = true;
            return false;
        }

        // 网络流错误 — 尝试重连
        if (isStream()) {
            return reconnectAndRead(packet);
        }
        return false;
    }
    return true;
}

bool Demuxer::reconnectAndRead(AVPacket* packet) {
    for (int attempt = 0; attempt < netConfig_.maxRetries; ++attempt) {
        int delayMs = netConfig_.retryBaseDelayMs * (1 << attempt);
        std::string msg = "Reconnecting attempt " + std::to_string(attempt + 1) +
            "/" + std::to_string(netConfig_.maxRetries);
        setConnState(ConnectionState::Reconnecting, msg);

        // 退避等待
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        // 关闭旧连接（保留配置）
        if (formatCtx_) {
            avformat_close_input(&formatCtx_);
            formatCtx_ = nullptr;
        }
        opened_ = false;
        eof_ = false;

        // 尝试重新打开
        formatCtx_ = avformat_alloc_context();
        if (!formatCtx_) continue;

        AVDictionary* opts = nullptr;
        av_dict_set(&opts, "timeout", std::to_string(netConfig_.timeoutUs).c_str(), 0);
        av_dict_set(&opts, "buffer_size", std::to_string(netConfig_.bufferSize).c_str(), 0);
        av_dict_set(&opts, "max_delay", std::to_string(netConfig_.maxDelay).c_str(), 0);
        av_dict_set(&opts, "fflags", "nobuffer", 0);

        int ret = avformat_open_input(&formatCtx_, currentUrl_.c_str(), nullptr, &opts);
        av_dict_free(&opts);

        if (ret < 0) {
            avformat_free_context(formatCtx_);
            formatCtx_ = nullptr;
            continue;
        }

        if (avformat_find_stream_info(formatCtx_, nullptr) < 0) {
            avformat_close_input(&formatCtx_);
            formatCtx_ = nullptr;
            continue;
        }

        opened_ = true;
        setConnState(ConnectionState::Connected, currentUrl_);

        // 重连成功，尝试读取
        ret = av_read_frame(formatCtx_, packet);
        if (ret >= 0) return true;

        // 读取出错，继续重试
        continue;
    }

    setConnState(ConnectionState::Failed,
        "Failed after " + std::to_string(netConfig_.maxRetries) + " retries");
    return false;
}

bool Demuxer::reopenInternal() {
    close();
    return open(currentUrl_, netConfig_);
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
    if (isStream()) return false;  // 网络流不支持 seek

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
        std::strstr(formatCtx_->iformat->name, "rtsp") != nullptr ||
        std::strstr(formatCtx_->iformat->name, "http") != nullptr ||
        formatCtx_->pb == nullptr ||
        !formatCtx_->pb->seekable
    );
}

double Demuxer::duration() const {
    if (!opened_ || !formatCtx_) return 0.0;
    return static_cast<double>(formatCtx_->duration) / AV_TIME_BASE;
}
```

在 `Demuxer.h` 的 `private:` 区域新增:
```cpp
    std::string currentUrl_;
    bool reconnectAndRead(AVPacket* packet);
```

同时在 `Demuxer.h` 顶部添加 `#include <thread>` 和 `#include <chrono>`:
```cpp
#include <string>
#include <atomic>
#include <thread>
#include <chrono>
```

- [ ] **Step 3: Commit**

```bash
git add src/core/demuxer/Demuxer.h src/core/demuxer/Demuxer.cpp
git commit -m "feat(rtmp): implement auto-reconnection with exponential backoff in Demuxer"
```

---

### Task 1.3: PlayerController 透传 RTMP 接口

**Files:**
- Modify: `src/core/controller/PlayerController.h`
- Modify: `src/core/controller/PlayerController.cpp`

- [ ] **Step 1: 修改 PlayerController.h — 新增透传接口**

在 `#include "core/audio/IAudioOutput.h"` 之后添加:
```cpp
#include "core/demuxer/NetworkConfig.h"
```

在 `bool open(const std::string& url);` 之后添加:
```cpp
    bool open(const std::string& url, const NetworkConfig& config);
```

在 `using ErrorCallback` 之后添加:
```cpp
    using ConnectionCallback = std::function<void(ConnectionState, const std::string&)>;
    void setConnectionCallback(ConnectionCallback cb);
```

在 `double currentPosition() const;` 之后添加:
```cpp
    ConnectionState connectionState() const;
```

- [ ] **Step 2: 修改 PlayerController.cpp — 实现透传**

在 `bool PlayerController::open(const std::string& url) {` 方法之前插入新重载:
```cpp
bool PlayerController::open(const std::string& url, const NetworkConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ != Idle && state_ != Stopped) {
        close();
    }

    if (!demuxer_->open(url, config)) {
        if (errorCb_) errorCb_("Failed to open: " + url);
        return false;
    }

    currentUrl_ = url;

    if (demuxer_->getVideoStreamIndex() >= 0) {
        void* device = nullptr;
        if (renderer_) device = renderer_->getNativeDevice();

        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto,
            device
        );
        if (!videoDecoder_) {
            if (errorCb_) errorCb_("Failed to create video decoder");
            return false;
        }
    }

    if (demuxer_->getAudioStreamIndex() >= 0) {
        audioDecoder_ = DecoderFactory::createAudioDecoder(
            demuxer_->getAudioParams()
        );
    }

    if (audioOutput_ && demuxer_->getAudioStreamIndex() >= 0) {
        auto* aparams = demuxer_->getAudioParams();
        audioOutput_->init(aparams->sample_rate, aparams->ch_layout.nb_channels, 2);
    }

    // 初始化音频变速器
    if (demuxer_->getAudioStreamIndex() >= 0) {
        initAudioResampler();
    }

    setState(Stopped);
    return true;
}
```

在文件末尾（`setState` 方法之后）添加:
```cpp
void PlayerController::setConnectionCallback(ConnectionCallback cb) {
    if (demuxer_) {
        demuxer_->setConnectionCallback(std::move(cb));
    }
}

ConnectionState PlayerController::connectionState() const {
    return demuxer_ ? demuxer_->connectionState() : ConnectionState::Disconnected;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/controller/PlayerController.h src/core/controller/PlayerController.cpp
git commit -m "feat(rtmp): add NetworkConfig overloads and connection state passthrough in PlayerController"
```

---

## Phase 2: 硬件解码纹理渲染打通

### Task 2.1: 新增 NativeTexture 结构体

**Files:**
- Modify: `src/core/common/VideoFrame.h`

- [ ] **Step 1: 在 VideoFrame.h 中新增 NativeTexture 和字段**

完整替换 `src/core/common/VideoFrame.h`:

```cpp
#pragma once

#include <cstdint>
#include <vector>

// 硬解平台纹理的统一表示
struct NativeTexture {
    enum Type {
        D3D11_TEXTURE,              // Windows: ID3D11Texture2D*
        GL_TEXTURE_EXTERNAL_OES,    // Android: GLuint
        ANDROID_SURFACE_DIRECT      // Android: MediaCodec 直接渲染到 Surface
    };

    Type type = D3D11_TEXTURE;
    void* handle = nullptr;     // D3D11: ID3D11Texture2D*, Android: GLuint (cast)
    int width = 0;
    int height = 0;
    int index = 0;              // D3D11VA 纹理数组索引
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

    // YUV平面数据 (软解)
    std::vector<uint8_t> data[3];  // Y, U, V
    int linesize[3] = {0, 0, 0};

    // 或 RGBA packed
    std::vector<uint8_t> rgbaData;

    // 平台纹理 (硬解)
    NativeTexture nativeTex;

    double pts = 0.0;           // 显示时间戳 (秒)
    double duration = 0.0;
};
```

- [ ] **Step 2: Commit**

```bash
git add src/core/common/VideoFrame.h
git commit -m "feat(hwdec): add NativeTexture struct for cross-platform hardware texture representation"
```

---

### Task 2.2: 更新 IRenderer 接口

**Files:**
- Modify: `src/core/renderer/IRenderer.h`

- [ ] **Step 1: 更新 renderTexture 签名并新增 getNativeDevice**

完整替换 `src/core/renderer/IRenderer.h`:

```cpp
#pragma once

#include "core/common/VideoFrame.h"

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(void* nativeWindow) = 0;
    virtual bool renderFrame(const VideoFrame& frame) = 0;
    virtual bool renderTexture(const NativeTexture& texture) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void destroy() = 0;

    // 返回平台原生设备（Windows: ID3D11Device*, Android: nullptr）
    virtual void* getNativeDevice() const { return nullptr; }
    virtual void* getNativeDeviceContext() const { return nullptr; }
};
```

- [ ] **Step 2: Commit**

```bash
git add src/core/renderer/IRenderer.h
git commit -m "feat(hwdec): update IRenderer with NativeTexture and device accessors"
```

---

### Task 2.3: 更新 DecoderFactory 支持设备传递

**Files:**
- Modify: `src/core/decoder/DecoderFactory.h`
- Modify: `src/core/decoder/DecoderFactory.cpp`

- [ ] **Step 1: 修改 DecoderFactory.h**

将 `HardwareCreator` 和 `createVideoDecoder` 改为接受 sharedDevice 参数。完整替换:

```cpp
#pragma once

#include "core/decoder/IDecoder.h"
#include <memory>
#include <string>
#include <functional>

class DecoderFactory {
public:
    enum class DecoderType {
        Auto,
        Software,
        Hardware
    };

    // sharedDevice: Windows=ID3D11Device*, Android=jobject Surface (cast to void*)
    using HardwareCreator = std::function<std::unique_ptr<IDecoder>(void* sharedDevice)>;

    static std::unique_ptr<IDecoder> createVideoDecoder(
        const AVCodecParameters* params,
        DecoderType type = DecoderType::Auto,
        void* sharedDevice = nullptr
    );

    static std::unique_ptr<IDecoder> createAudioDecoder(
        const AVCodecParameters* params
    );

    static bool isHardwareDecodeAvailable(const AVCodecParameters* params);

    static void registerHardwareCreator(HardwareCreator creator);

private:
    static HardwareCreator& getHardwareCreator();
};
```

- [ ] **Step 2: 修改 DecoderFactory.cpp**

完整替换:

```cpp
#include "core/decoder/DecoderFactory.h"
#include "core/decoder/SoftwareDecoder.h"

DecoderFactory::HardwareCreator& DecoderFactory::getHardwareCreator() {
    static HardwareCreator creator;
    return creator;
}

void DecoderFactory::registerHardwareCreator(HardwareCreator creator) {
    getHardwareCreator() = std::move(creator);
}

std::unique_ptr<IDecoder> DecoderFactory::createVideoDecoder(
    const AVCodecParameters* params,
    DecoderType type,
    void* sharedDevice
) {
    if (type == DecoderType::Hardware || type == DecoderType::Auto) {
        auto& hwCreator = getHardwareCreator();
        if (hwCreator) {
            auto decoder = hwCreator(sharedDevice);
            if (decoder && decoder->init(params)) {
                return decoder;
            }
        }
    }

    // 回退到软解
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
    auto& hwCreator = getHardwareCreator();
    return static_cast<bool>(hwCreator);
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/decoder/DecoderFactory.h src/core/decoder/DecoderFactory.cpp
git commit -m "feat(hwdec): add sharedDevice parameter to DecoderFactory for hardware decoder device sharing"
```

---

### Task 2.4: Windows — D3D11Renderer 暴露设备 + NV12 渲染

**Files:**
- Modify: `src/platform/windows/D3D11Renderer.h`
- Modify: `src/platform/windows/D3D11Renderer.cpp`

- [ ] **Step 1: 修改 D3D11Renderer.h — 新增成员和方法**

在 `public:` 区域 `destroy()` 之后添加:
```cpp
    void* getNativeDevice() const override { return device_.Get(); }
    void* getNativeDeviceContext() const override { return context_.Get(); }
```

在 `private:` 区域新增 NV12 渲染相关成员（在 `bool initialized_ = false;` 之前）:
```cpp
    // NV12 硬解渲染
    ComPtr<ID3D11PixelShader> nv12PixelShader_;
    ComPtr<ID3D11Texture2D> nv12Texture_;
    ComPtr<ID3D11ShaderResourceView> nv12YSRV_;
    ComPtr<ID3D11ShaderResourceView> nv12UVSRV_;
    int hwTexWidth_ = 0;
    int hwTexHeight_ = 0;

    bool createNV12Shader();
    bool createNV12SRVs(ID3D11Texture2D* srcTexture, int index, int width, int height);
```

将 `bool renderTexture(void* nativeTexture, int width, int height) override;` 替换为:
```cpp
    bool renderTexture(const NativeTexture& texture) override;
```

- [ ] **Step 2: 修改 D3D11Renderer.cpp — 实现 NV12 渲染**

在 `createVertexBuffer()` 方法之后、`renderFrame()` 方法之前，添加:

```cpp
bool D3D11Renderer::createNV12Shader() {
    const char* nv12PsSource = R"(
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
    HRESULT hr = D3DCompile(nv12PsSource, strlen(nv12PsSource), nullptr, nullptr, nullptr,
        "PS", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) return false;

    return SUCCEEDED(device_->CreatePixelShader(psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(), nullptr, &nv12PixelShader_));
}

bool D3D11Renderer::createNV12SRVs(ID3D11Texture2D* srcTexture, int index, int width, int height) {
    if (hwTexWidth_ == width && hwTexHeight_ == height && nv12YSRV_ && nv12UVSRV_) {
        // 尺寸没变，SRV 已创建，直接复用
        return true;
    }

    nv12YSRV_.Reset();
    nv12UVSRV_.Reset();

    // Y 平面 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC yDesc = {};
    yDesc.Format = DXGI_FORMAT_R8_UNORM;
    yDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    yDesc.Texture2DArray.MostDetailedMip = 0;
    yDesc.Texture2DArray.MipLevels = 1;
    yDesc.Texture2DArray.FirstArraySlice = index;
    yDesc.Texture2DArray.ArraySize = 1;

    HRESULT hr = device_->CreateShaderResourceView(srcTexture, &yDesc, &nv12YSRV_);
    if (FAILED(hr)) return false;

    // UV 平面 SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC uvDesc = {};
    uvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
    uvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    uvDesc.Texture2DArray.MostDetailedMip = 0;
    uvDesc.Texture2DArray.MipLevels = 1;
    uvDesc.Texture2DArray.FirstArraySlice = index;
    uvDesc.Texture2DArray.ArraySize = 1;

    hr = device_->CreateShaderResourceView(srcTexture, &uvDesc, &nv12UVSRV_);
    if (FAILED(hr)) return false;

    hwTexWidth_ = width;
    hwTexHeight_ = height;
    return true;
}
```

在 `init()` 方法中，`initialized_ = true;` 之前添加:
```cpp
    if (!createNV12Shader()) return false;
```

将 `renderTexture` 方法完整替换:
```cpp
bool D3D11Renderer::renderTexture(const NativeTexture& texture) {
    if (!initialized_) return false;
    if (texture.type != NativeTexture::D3D11_TEXTURE || !texture.handle) return false;

    auto* srcTexture = static_cast<ID3D11Texture2D*>(texture.handle);

    if (!createNV12SRVs(srcTexture, texture.index, texture.width, texture.height)) {
        return false;
    }

    float clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    ID3D11ShaderResourceView* srvs[] = { nv12YSRV_.Get(), nv12UVSRV_.Get() };
    context_->PSSetShaderResources(0, 2, srvs);
    context_->PSSetSamplers(0, 1, samplerState_.GetAddressOf());

    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(nv12PixelShader_.Get(), nullptr, 0);

    UINT stride = sizeof(float) * 4;
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context_->Draw(4, 0);

    swapChain_->Present(1, 0);
    return true;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/windows/D3D11Renderer.h src/platform/windows/D3D11Renderer.cpp
git commit -m "feat(hwdec): implement D3D11 NV12 texture rendering and expose device for sharing"
```

---

### Task 2.5: Windows — D3D11VAHardwareDecoder 共享设备

**Files:**
- Modify: `src/platform/windows/D3D11VAHardwareDecoder.h`
- Modify: `src/platform/windows/D3D11VAHardwareDecoder.cpp`

- [ ] **Step 1: 修改 D3D11VAHardwareDecoder.h — 新增共享设备支持**

在 `public:` 区域 `AVPixelFormat outputFormat() const override;` 之后添加:
```cpp
    // 设置共享 D3D11 设备（必须在 init() 之前调用）
    void setSharedDevice(ID3D11Device* device, ID3D11DeviceContext* context);

    // 获取最近解码的纹理信息
    bool getLastNativeTexture(NativeTexture& tex) const;
```

在 `private:` 区域 `bool initialized_ = false;` 之前添加:
```cpp
    ID3D11Device* sharedDevice_ = nullptr;
    ID3D11DeviceContext* sharedContext_ = nullptr;
    ComPtr<ID3D11Texture2D> lastTexture_;
    int lastIndex_ = 0;
    int lastWidth_ = 0;
    int lastHeight_ = 0;
```

- [ ] **Step 2: 修改 D3D11VAHardwareDecoder.cpp — 使用共享设备**

完整替换:

```cpp
#include "D3D11VAHardwareDecoder.h"

extern "C" {
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}

void D3D11VAHardwareDecoder::setSharedDevice(ID3D11Device* device, ID3D11DeviceContext* context) {
    sharedDevice_ = device;
    sharedContext_ = context;
}

bool D3D11VAHardwareDecoder::getLastNativeTexture(NativeTexture& tex) const {
    if (!lastTexture_) return false;
    tex.type = NativeTexture::D3D11_TEXTURE;
    tex.handle = lastTexture_.Get();
    tex.index = lastIndex_;
    tex.width = lastWidth_;
    tex.height = lastHeight_;
    return true;
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

    if (sharedDevice_ && sharedContext_) {
        // 使用共享设备 — 避免创建独立设备
        AVBufferRef* hwRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (!hwRef) {
            destroy();
            return false;
        }

        auto* devCtx = reinterpret_cast<AVHWDeviceContext*>(hwRef->data);
        auto* d3d11Ctx = reinterpret_cast<AVD3D11VADeviceContext*>(devCtx->hwctx);

        // AddRef 以防 FFmpeg 在 unref 时 Release
        sharedDevice_->AddRef();
        sharedContext_->AddRef();
        d3d11Ctx->device = sharedDevice_;
        d3d11Ctx->device_ctx = sharedContext_;
        d3d11Ctx->lock = nullptr;
        d3d11Ctx->unlock = nullptr;

        if (av_hwdevice_ctx_init(hwRef) < 0) {
            av_buffer_unref(&hwRef);
            destroy();
            return false;
        }

        hwDeviceCtx_ = hwRef;
    } else {
        // 无共享设备 — 创建独立设备（兼容旧逻辑）
        int ret = av_hwdevice_ctx_create(&hwDeviceCtx_, AV_HWDEVICE_TYPE_D3D11VA,
            nullptr, nullptr, 0);
        if (ret < 0) {
            destroy();
            return false;
        }
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

    lastWidth_ = params->width;
    lastHeight_ = params->height;
    initialized_ = true;
    return true;
}

bool D3D11VAHardwareDecoder::decode(const AVPacket* packet, AVFrame* frame) {
    if (!codecCtx_) return false;

    int ret = avcodec_send_packet(codecCtx_, packet);
    if (ret < 0) return false;

    ret = avcodec_receive_frame(codecCtx_, frame);
    if (ret < 0) return false;

    if (frame->format == AV_PIX_FMT_D3D11) {
        // D3D11 帧格式: data[0] = AVD3D11FrameDescriptor*, data[1] = index (as ptr)
        auto* desc = reinterpret_cast<AVD3D11FrameDescriptor*>(frame->data[0]);
        if (desc && desc->texture) {
            lastTexture_ = desc->texture;
            lastIndex_ = desc->index;
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
    lastTexture_.Reset();
    codec_ = nullptr;
    sharedDevice_ = nullptr;
    sharedContext_ = nullptr;
    initialized_ = false;
}

AVPixelFormat D3D11VAHardwareDecoder::outputFormat() const {
    return AV_PIX_FMT_D3D11;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/windows/D3D11VAHardwareDecoder.h src/platform/windows/D3D11VAHardwareDecoder.cpp
git commit -m "feat(hwdec): add shared device support to D3D11VAHardwareDecoder"
```

---

### Task 2.6: Windows — CMakeLists + 硬解注册 + 设备传递

**Files:**
- Modify: `src/platform/windows/CMakeLists.txt`
- Modify: `src/platform/windows/WinMainWindow.cpp`

- [ ] **Step 1: 修改 Windows CMakeLists — 补充 D3D11VAHardwareDecoder**

替换 `src/platform/windows/CMakeLists.txt`:

```cmake
add_executable(MPlayerWin WIN32
    main.cpp
    WinMainWindow.cpp
    D3D11Renderer.cpp
    D3D11VAHardwareDecoder.cpp
    WinAudioOutput.cpp
)

target_link_libraries(MPlayerWin PRIVATE
    MPlayerCore
    d3d11
    dxgi
    d3dcompiler
    dxguid
    imm32
    ole32
)
```

- [ ] **Step 2: 修改 WinMainWindow.cpp — 注册硬解工厂**

在 `#include "WinAudioOutput.h"` 之后添加:
```cpp
#include "D3D11VAHardwareDecoder.h"
#include "core/decoder/DecoderFactory.h"
```

在 `WinMainWindow::init()` 中，`auto renderer = std::make_unique<D3D11Renderer>();` 之前添加:
```cpp
    // 注册硬件解码器工厂（之后创建 Renderer 时需要拿到设备）
    // 注意：先创建 Renderer 再注册，因为工厂需要从 Renderer 获取设备
```

在 `player_->setAudioOutput(std::move(audio));` 之后添加:
```cpp
    // 注册硬件解码器创建工厂
    auto* rawRenderer = renderer.get();  // 保存裸指针用于工厂
    DecoderFactory::registerHardwareCreator([rawRenderer](void* sharedDevice) ->
        std::unique_ptr<IDecoder> {
        auto decoder = std::make_unique<D3D11VAHardwareDecoder>();
        if (auto* dev = static_cast<ID3D11Device*>(rawRenderer->getNativeDevice())) {
            auto* ctx = static_cast<ID3D11DeviceContext*>(rawRenderer->getNativeDeviceContext());
            decoder->setSharedDevice(dev, ctx);
        }
        return decoder;
    });
```

注意：`auto renderer = std::make_unique<D3D11Renderer>();` 需要在注册之前，因为工厂 lambda 捕获了裸指针。当前代码顺序已经是先创建 renderer 再 setRenderer，只需在 setRenderer 之后注册工厂即可。

同时需要添加头文件引用，在文件顶部已有 `#include "D3D11Renderer.h"`。

- [ ] **Step 3: Commit**

```bash
git add src/platform/windows/CMakeLists.txt src/platform/windows/WinMainWindow.cpp
git commit -m "feat(hwdec): register D3D11VA hardware decoder factory in Windows platform"
```

---

### Task 2.7: Android — CMakeLists + GLESRenderer 硬解渲染

**Files:**
- Modify: `src/platform/android/CMakeLists.txt`
- Modify: `src/platform/android/GLESRenderer.h`
- Modify: `src/platform/android/GLESRenderer.cpp`

- [ ] **Step 1: 修改 Android CMakeLists — 补充 MediaCodec + mediandk**

替换 `src/platform/android/CMakeLists.txt`:

```cmake
add_library(MPlayerAndroid SHARED
    jni/MPlayerJNI.cpp
    GLESRenderer.cpp
    MediaCodecDecoder.cpp
    AndroidAudioOutput.cpp
)

target_include_directories(MPlayerAndroid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(MPlayerAndroid PRIVATE
    MPlayerCore
    EGL
    GLESv3
    OpenSLES
    mediandk
    android
    log
)
```

- [ ] **Step 2: 修改 GLESRenderer.h — 更新 renderTexture 签名**

将 `bool renderTexture(void* nativeTexture, int width, int height) override;` 替换为:
```cpp
    bool renderTexture(const NativeTexture& texture) override;
```

- [ ] **Step 3: 修改 GLESRenderer.cpp — 实现 renderTexture**

将 `renderTexture` 方法替换为:
```cpp
bool GLESRenderer::renderTexture(const NativeTexture& texture) {
    if (!initialized_) return false;

    if (texture.type == NativeTexture::ANDROID_SURFACE_DIRECT) {
        // MediaCodec 已直接渲染到 Surface，无需 GLES 操作
        // 仅需 swap buffers 以保持渲染循环一致
        eglSwapBuffers(display_, surface_);
        return true;
    }

    // 未来可扩展: GL_TEXTURE_EXTERNAL_OES 纹理渲染
    return false;
}
```

- [ ] **Step 4: Commit**

```bash
git add src/platform/android/CMakeLists.txt src/platform/android/GLESRenderer.h src/platform/android/GLESRenderer.cpp
git commit -m "feat(hwdec): add MediaCodec to Android build and implement surface-direct renderTexture"
```

---

### Task 2.8: Android — MediaCodecDecoder Surface 输出完善

**Files:**
- Modify: `src/platform/android/MediaCodecDecoder.h`
- Modify: `src/platform/android/MediaCodecDecoder.cpp`

- [ ] **Step 1: 修改 MediaCodecDecoder.h — 新增全局 Surface 管理和 NativeTexture 输出**

在 `private:` 区域 `bool configured_ = false;` 之前添加:
```cpp
    JNIEnv* jniEnv_ = nullptr;
    jobject globalSurface_ = nullptr;
```

在 `public:` 区域 `void setSurface(jobject surface);` 之后添加:
```cpp
    // 设置 JNI 环境（用于创建全局引用）
    void setJniEnv(JNIEnv* env);

    // 获取最近解码帧的 NativeTexture 信息
    bool getLastNativeTexture(NativeTexture& tex) const;
```

- [ ] **Step 2: 修改 MediaCodecDecoder.cpp — 完善 Surface 管理**

将 `setSurface` 方法替换为:
```cpp
void MediaCodecDecoder::setSurface(jobject surface) {
    if (globalSurface_ && jniEnv_) {
        jniEnv_->DeleteGlobalRef(globalSurface_);
        globalSurface_ = nullptr;
    }
    surface_ = surface;
}

void MediaCodecDecoder::setJniEnv(JNIEnv* env) {
    jniEnv_ = env;
}

bool MediaCodecDecoder::getLastNativeTexture(NativeTexture& tex) const {
    // MediaCodec 直接渲染模式下，帧已输出到 Surface
    tex.type = NativeTexture::ANDROID_SURFACE_DIRECT;
    tex.handle = nullptr;
    tex.width = width_;
    tex.height = height_;
    tex.index = 0;
    return configured_;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/platform/android/MediaCodecDecoder.h src/platform/android/MediaCodecDecoder.cpp
git commit -m "feat(hwdec): add JNI env and NativeTexture output support to MediaCodecDecoder"
```

---

### Task 2.9: Android — MPlayerJNI 注册硬解 + Surface 传递

**Files:**
- Modify: `src/platform/android/jni/MPlayerJNI.cpp`

- [ ] **Step 1: 修改 MPlayerJNI.cpp — 注册硬件解码器、传递 Surface**

在 `#include "AndroidAudioOutput.h"` 之后添加:
```cpp
#include "MediaCodecDecoder.h"
#include "core/decoder/DecoderFactory.h"
```

在 `MPlayerJNI::nativeSetSurface` 方法中，`player->setAudioOutput(std::move(audio));` 之后添加:
```cpp
    // 注册硬件解码器工厂
    // 注意：每次 setSurface 可能对应不同的 player 实例，需要更新工厂
    // 保存 Surface 作为全局引用
    static jobject g_surface = nullptr;
    static JNIEnv* g_env = env;

    if (g_surface) {
        g_env->DeleteGlobalRef(g_surface);
    }
    g_surface = env->NewGlobalRef(surface);

    DecoderFactory::registerHardwareCreator([g_surface](void* sharedDevice) ->
        std::unique_ptr<IDecoder> {
        auto decoder = std::make_unique<MediaCodecDecoder>();
        decoder->setJniEnv(g_env);  // NOLINT: 捕获静态变量
        if (g_surface) {
            decoder->setSurface(g_surface);
        }
        return decoder;
    });
```

- [ ] **Step 2: Commit**

```bash
git add src/platform/android/jni/MPlayerJNI.cpp
git commit -m "feat(hwdec): register MediaCodec hardware decoder factory in Android JNI"
```

---

### Task 2.10: PlayerController — 硬解帧路径 + 设备传递

**Files:**
- Modify: `src/core/controller/PlayerController.h`
- Modify: `src/core/controller/PlayerController.cpp`

- [ ] **Step 1: 修改 PlayerController.h — 新增设备传递和硬解支持**

在 `public:` 区域 `void setAudioOutput(...)` 之后添加:
```cpp
    // 设置平台原生设备（用于硬件解码器初始化）
    void setNativeDevice(void* device) { nativeDevice_ = device; }
```

在 `private:` 区域 `ErrorCallback errorCb_;` 之前添加:
```cpp
    void* nativeDevice_ = nullptr;
    std::unique_ptr<AudioResampler> audioResampler_;
    mutable std::mutex frameMutex_;
    VideoFrame lastFrame_;
```

在 `private:` 区域新增辅助方法:
```cpp
    void initAudioResampler();
```

- [ ] **Step 2: 修改 PlayerController.cpp — 更新 open 和 videoDecodeThread**

将现有的 `bool PlayerController::open(const std::string& url)` 方法修改为使用 DecoderFactory 的新签名。替换方法体为:

```cpp
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

    if (demuxer_->getVideoStreamIndex() >= 0) {
        void* device = nullptr;
        if (renderer_) device = renderer_->getNativeDevice();

        videoDecoder_ = DecoderFactory::createVideoDecoder(
            demuxer_->getVideoParams(),
            DecoderFactory::DecoderType::Auto,
            device
        );
        if (!videoDecoder_) {
            if (errorCb_) errorCb_("Failed to create video decoder");
            return false;
        }
    }

    if (demuxer_->getAudioStreamIndex() >= 0) {
        audioDecoder_ = DecoderFactory::createAudioDecoder(
            demuxer_->getAudioParams()
        );
    }

    if (audioOutput_ && demuxer_->getAudioStreamIndex() >= 0) {
        auto* aparams = demuxer_->getAudioParams();
        audioOutput_->init(aparams->sample_rate, aparams->ch_layout.nb_channels, 2);
    }

    // 初始化音频变速器
    if (demuxer_->getAudioStreamIndex() >= 0) {
        initAudioResampler();
    }

    setState(Stopped);
    return true;
}
```

修改 `videoDecodeThread()` — 在 `if (videoDecoder_->decode(packet, frame)) {` 块内，将软解逻辑包裹在条件判断中。完整替换 `videoDecodeThread()`:

```cpp
void PlayerController::videoDecodeThread() {
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

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getVideoStreamIndex()]->time_base;
            vf.pts = frame->pts * av_q2d(timeBase);

            if (videoDecoder_->isHardware()) {
                // 硬件解码路径 — NativeTexture
                vf.format = VideoFrame::NativeTexture;

                // 从平台解码器获取 NativeTexture
                // Windows: D3D11VA, Android: MediaCodec
                // 通过 IDecoder 的扩展接口获取（dynamic_cast 到具体类型）
#ifdef _WIN32
                auto* d3d11dec = dynamic_cast<D3D11VAHardwareDecoder*>(videoDecoder_.get());
                if (d3d11dec) {
                    d3d11dec->getLastNativeTexture(vf.nativeTex);
                }
#elif defined(__ANDROID__)
                auto* mcdec = dynamic_cast<MediaCodecDecoder*>(videoDecoder_.get());
                if (mcdec) {
                    mcdec->getLastNativeTexture(vf.nativeTex);
                }
#endif
            } else {
                // 软件解码路径 — YUV 数据
                if (frame->format == AV_PIX_FMT_YUV420P) {
                    vf.format = VideoFrame::YUV420P;
                    for (int i = 0; i < 3; i++) {
                        vf.linesize[i] = frame->linesize[i];
                        int h = (i == 0) ? frame->height : frame->height / 2;
                        int size = frame->linesize[i] * h;
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
            }

            if (videoFrameCb_) {
                videoFrameCb_(vf);
            }

            // 缓存最新帧（供截图使用）
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                lastFrame_ = vf;
            }

            videoFrameQueue_.push(std::move(vf));
            currentPosition_ = vf.pts;
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}
```

在文件顶部添加平台头文件条件引用:
```cpp
#ifdef _WIN32
#include "D3D11VAHardwareDecoder.h"
#elif defined(__ANDROID__)
#include "MediaCodecDecoder.h"
#endif
```

在文件末尾添加空实现（Phase 3 会填充）:
```cpp
void PlayerController::initAudioResampler() {
    // Phase 3: AudioResampler 初始化
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/controller/PlayerController.h src/core/controller/PlayerController.cpp
git commit -m "feat(hwdec): add hardware decode frame path and device passthrough in PlayerController"
```

---

## Phase 3: 高级功能（倍速 / 截图）

### Task 3.1: 添加 stb_image_write

**Files:**
- Create: `third_party/stb/stb_image_write.h`

- [ ] **Step 1: 下载 stb_image_write.h**

```bash
curl -L -o third_party/stb/stb_image_write.h https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h
```

如果网络不可用，手动创建文件并从 https://github.com/nothings/stb/raw/master/stb_image_write.h 下载内容。

- [ ] **Step 2: Commit**

```bash
git add third_party/stb/stb_image_write.h
git commit -m "chore: add stb_image_write.h for PNG screenshot support"
```

---

### Task 3.2: 创建 AudioResampler

**Files:**
- Create: `src/core/audio/AudioResampler.h`
- Create: `src/core/audio/AudioResampler.cpp`

- [ ] **Step 1: 创建 AudioResampler.h**

```cpp
// src/core/audio/AudioResampler.h
#pragma once

extern "C" {
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

#include <cstdint>
#include <vector>

class AudioResampler {
public:
    AudioResampler() = default;
    ~AudioResampler();

    bool init(int sampleRate, int channels, AVSampleFormat format);
    bool setSpeed(float speed);
    float getSpeed() const { return speed_; }
    bool process(const AVFrame* input, std::vector<uint8_t>& output);
    void destroy();

private:
    AVFilterGraph* filterGraph_ = nullptr;
    AVFilterContext* srcCtx_ = nullptr;
    AVFilterContext* atempoCtx_ = nullptr;
    AVFilterContext* sinkCtx_ = nullptr;

    int sampleRate_ = 0;
    int channels_ = 0;
    AVSampleFormat format_ = AV_SAMPLE_FMT_NONE;
    float speed_ = 1.0f;
    bool initialized_ = false;
};
```

- [ ] **Step 2: 创建 AudioResampler.cpp**

```cpp
// src/core/audio/AudioResampler.cpp
#include "core/audio/AudioResampler.h"
#include <cstdio>

AudioResampler::~AudioResampler() {
    destroy();
}

bool AudioResampler::init(int sampleRate, int channels, AVSampleFormat format) {
    sampleRate_ = sampleRate;
    channels_ = channels;
    format_ = format;

    filterGraph_ = avfilter_graph_alloc();
    if (!filterGraph_) return false;

    // abuffer 源
    const AVFilter* abuffer = avfilter_get_by_name("abuffer");
    if (!abuffer) { destroy(); return false; }

    char args[512];
    snprintf(args, sizeof(args),
        "sample_rate=%d:sample_fmt=%s:channels=%d:time_base=1/%d",
        sampleRate,
        av_get_sample_fmt_name(format),
        channels,
        sampleRate);

    int ret = avfilter_graph_create_filter(&srcCtx_, abuffer, "src",
        args, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    // atempo 滤镜
    const AVFilter* atempo = avfilter_get_by_name("atempo");
    if (!atempo) { destroy(); return false; }

    char tempoArgs[32];
    snprintf(tempoArgs, sizeof(tempoArgs), "tempo=%f", 1.0);

    ret = avfilter_graph_create_filter(&atempoCtx_, atempo, "atempo",
        tempoArgs, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    // abuffersink
    const AVFilter* sink = avfilter_get_by_name("abuffersink");
    if (!sink) { destroy(); return false; }

    ret = avfilter_graph_create_filter(&sinkCtx_, sink, "sink",
        nullptr, nullptr, filterGraph_);
    if (ret < 0) { destroy(); return false; }

    // 连接: src → atempo → sink
    ret = avfilter_link(srcCtx_, 0, atempoCtx_, 0);
    if (ret < 0) { destroy(); return false; }

    ret = avfilter_link(atempoCtx_, 0, sinkCtx_, 0);
    if (ret < 0) { destroy(); return false; }

    // 配置滤镜图
    ret = avfilter_graph_config(filterGraph_, nullptr);
    if (ret < 0) { destroy(); return false; }

    initialized_ = true;
    return true;
}

bool AudioResampler::setSpeed(float speed) {
    if (!atempoCtx_ || speed < 0.5f || speed > 100.0f) return false;
    speed_ = speed;

    char str[32];
    snprintf(str, sizeof(str), "%f", speed);
    return av_opt_set(atempoCtx_->priv, "tempo", str, AV_OPT_SEARCH_CHILDREN) >= 0;
}

bool AudioResampler::process(const AVFrame* input, std::vector<uint8_t>& output) {
    if (!initialized_ || !srcCtx_ || !sinkCtx_) return false;

    // 将输入帧推入源
    int ret = av_buffersrc_add_frame_flags(srcCtx_,
        const_cast<AVFrame*>(input), AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0) return false;

    // 从 sink 拉取输出
    AVFrame* filtFrame = av_frame_alloc();
    ret = av_buffersink_get_frame(sinkCtx_, filtFrame);
    if (ret < 0) {
        av_frame_free(&filtFrame);
        return false;
    }

    int dataSize = av_samples_get_buffer_size(nullptr,
        filtFrame->ch_layout.nb_channels,
        filtFrame->nb_samples,
        static_cast<AVSampleFormat>(filtFrame->format), 1);

    if (dataSize > 0 && filtFrame->data[0]) {
        output.assign(filtFrame->data[0], filtFrame->data[0] + dataSize);
    }

    av_frame_free(&filtFrame);
    return !output.empty();
}

void AudioResampler::destroy() {
    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
    }
    srcCtx_ = nullptr;
    atempoCtx_ = nullptr;
    sinkCtx_ = nullptr;
    initialized_ = false;
}
```

- [ ] **Step 3: Commit**

```bash
git add src/core/audio/AudioResampler.h src/core/audio/AudioResampler.cpp
git commit -m "feat(speed): add AudioResampler with FFmpeg atempo filter for audio speed control"
```

---

### Task 3.3: PlayerController — 实现倍速和截图

**Files:**
- Modify: `src/core/controller/PlayerController.h`
- Modify: `src/core/controller/PlayerController.cpp`
- Modify: `src/core/CMakeLists.txt`

- [ ] **Step 1: 修改 PlayerController.h — 新增截图接口**

在 `#include "core/audio/IAudioOutput.h"` 之后添加:
```cpp
#include "core/audio/AudioResampler.h"
```

在 `public:` 区域 `VideoInfo getVideoInfo() const;` 之后添加:
```cpp
    // 截图：将当前帧保存为 PNG
    bool captureFrame(const std::string& savePath);
```

注意：`audioResampler_`、`frameMutex_`、`lastFrame_` 已在 Task 2.10 中添加到 private 区域，无需重复声明。

- [ ] **Step 2: 修改 PlayerController.cpp — 实现变速和截图**

在文件顶部 `#include "core/controller/PlayerController.h"` 之后添加:
```cpp
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

extern "C" {
#include <libswscale/swscale.h>
}
```

替换 `setSpeed` 方法:
```cpp
void PlayerController::setSpeed(float speed) {
    speed_ = speed;
    if (audioResampler_) {
        audioResampler_->setSpeed(speed);
    }
}
```

替换 `audioDecodeThread`:
```cpp
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

            AVRational timeBase = demuxer_->getFormatContext()->
                streams[demuxer_->getAudioStreamIndex()]->time_base;
            af.pts = frame->pts * av_q2d(timeBase);

            int dataSize = av_samples_get_buffer_size(nullptr, af.channels, af.samples,
                static_cast<AVSampleFormat>(frame->format), 1);

            if (dataSize > 0 && frame->data[0]) {
                // 倍速处理
                if (audioResampler_ && speed_ != 1.0f) {
                    std::vector<uint8_t> resampled;
                    if (audioResampler_->process(frame, resampled)) {
                        af.data = std::move(resampled);
                        af.samples = static_cast<int>(af.data.size()) /
                            (af.channels * af.bytesPerSample);
                    } else {
                        af.data.assign(frame->data[0], frame->data[0] + dataSize);
                    }
                } else {
                    af.data.assign(frame->data[0], frame->data[0] + dataSize);
                }
                audioFrameQueue_.push(std::move(af));
            }
        }

        av_packet_free(&packet);
        av_frame_unref(frame);
    }

    av_frame_free(&frame);
}
```

替换 `initAudioResampler`:
```cpp
void PlayerController::initAudioResampler() {
    if (!demuxer_ || demuxer_->getAudioStreamIndex() < 0) return;

    auto* aparams = demuxer_->getAudioParams();
    if (!aparams) return;

    audioResampler_ = std::make_unique<AudioResampler>();
    if (!audioResampler_->init(
        aparams->sample_rate,
        aparams->ch_layout.nb_channels,
        static_cast<AVSampleFormat>(aparams->format))) {
        audioResampler_.reset();
    }
}
```

在文件末尾（`setState` 之后）添加截图方法:
```cpp
bool PlayerController::captureFrame(const std::string& savePath) {
    VideoFrame frame;
    {
        std::lock_guard<std::mutex> lock(frameMutex_);
        if (lastFrame_.format == VideoFrame::Format::NativeTexture) {
            // 硬解帧暂不支持截图（需要 GPU 回读）
            return false;
        }
        frame = lastFrame_;
    }

    if (frame.width <= 0 || frame.height <= 0) return false;

    // YUV420P → RGBA
    std::vector<uint8_t> rgbaData(frame.width * frame.height * 4);

    SwsContext* swsCtx = sws_getContext(
        frame.width, frame.height, AV_PIX_FMT_YUV420P,
        frame.width, frame.height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);

    if (!swsCtx) return false;

    const uint8_t* srcSlice[3] = {
        frame.data[0].data(),
        frame.data[1].data(),
        frame.data[2].data()
    };
    int srcStride[3] = { frame.linesize[0], frame.linesize[1], frame.linesize[2] };

    uint8_t* dstSlice[1] = { rgbaData.data() };
    int dstStride[1] = { frame.width * 4 };

    sws_scale(swsCtx, srcSlice, srcStride, 0, frame.height, dstSlice, dstStride);
    sws_freeContext(swsCtx);

    int result = stbi_write_png(savePath.c_str(), frame.width, frame.height,
        4, rgbaData.data(), frame.width * 4);

    return result != 0;
}
```

- [ ] **Step 3: 修改 core CMakeLists — 新增 AudioResampler**

替换 `src/core/CMakeLists.txt`:

```cmake
add_library(MPlayerCore STATIC
    demuxer/Demuxer.cpp
    decoder/SoftwareDecoder.cpp
    decoder/DecoderFactory.cpp
    renderer/RendererFactory.cpp
    controller/PlayerController.cpp
    audio/AudioResampler.cpp
)

target_include_directories(MPlayerCore PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party/stb
)

target_link_libraries(MPlayerCore PUBLIC MPlayerFFmpeg)
```

- [ ] **Step 4: 构建验证**

```bash
cd H:/MPlayer
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug 2>&1 | head -50
```

预期：编译通过，可能有少量 warning 但无 error。

- [ ] **Step 5: Commit**

```bash
git add src/core/controller/PlayerController.h src/core/controller/PlayerController.cpp src/core/audio/AudioResampler.h src/core/audio/AudioResampler.cpp src/core/CMakeLists.txt
git commit -m "feat: implement audio speed control via atempo filter and PNG screenshot capture"
```

---

## 最终整合 Commit

- [ ] **最终验证构建并提交**

```bash
# Windows 构建
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug

# 如有编译错误，修复后提交
git add -A
git commit -m "feat: complete RTMP reconnection, hardware decode rendering, and advanced features (speed/screenshot)"
```
