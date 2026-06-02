# MPlayer RTMP完善 / 硬解渲染 / 高级功能 设计文档

> **日期**: 2026-06-02
> **状态**: 待实现
> **依赖**: P0-P2 已完成，本设计覆盖 P3-P5 部分功能

## 概述

本文档定义三个独立但有序实现的功能模块：

1. **RTMP 流完善** — 自动重连与缓冲策略
2. **硬件解码纹理渲染打通** — Windows D3D11VA + Android MediaCodec 零拷贝渲染
3. **高级功能** — 音视频同步变速、PNG 截图、视频信息

实现顺序：模块 1 → 模块 2 → 模块 3。模块 3 依赖模块 2（截图需识别硬解帧）。

---

## 模块 1: RTMP 流完善

### 目标

为 RTMP/RTSP/HTTP 网络流添加基础自动重连和统一网络参数配置。

### 新增数据结构

**`src/core/demuxer/NetworkConfig.h`**:

```cpp
#pragma once
#include <functional>
#include <string>

struct NetworkConfig {
    int timeoutUs = 5000000;        // 网络超时 5秒
    int bufferSize = 1024000;       // 缓冲区大小 (bytes)
    int maxDelay = 500000;          // 最大延迟 (us)
    int maxRetries = 3;             // 最大重连次数
    int retryBaseDelayMs = 1000;    // 重连基础间隔 1秒
    bool isLive = true;             // 是否直播流
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

### Demuxer 改造

**新增方法**:

- `bool open(const std::string& url, const NetworkConfig& config)` — 带网络配置的打开
- `void setConnectionCallback(ConnectionCallback cb)` — 设置连接状态回调
- `ConnectionState connectionState() const` — 查询当前连接状态

**保持兼容**: 原 `open(const std::string& url)` 保持不变，内部使用默认 `NetworkConfig`。当使用默认配置时，`isLive` 由 URL scheme 自动推断（`rtmp://`/`rtsp://` 为 true，`http://`/本地文件为 false）。

**重连逻辑**（在 `readPacket()` 内部）:

1. `av_read_frame()` 返回错误时，调用 `isStream()` 判断是否为网络流
2. 如果是网络流且错误非 EOF，进入重连流程
3. 第 N 次重试等待 `retryBaseDelayMs * 2^(N-1)` 毫秒（指数退避）
4. 重连 = 调用 `close()` + 重新 `avformat_open_input()` + `avformat_find_stream_info()`
5. 重连期间通过回调通知 `ConnectionState::Reconnecting`
6. 重连成功后恢复 `ConnectionState::Connected`，继续读取
7. 超过 `maxRetries` 则通知 `ConnectionState::Failed`，返回 false

**seek 安全处理**: 网络直播流（`isLive == true`）调用 `seek()` 直接返回 false，不尝试 seek。

**新增成员**:

```cpp
NetworkConfig netConfig_;
ConnectionCallback connCb_;
std::atomic<ConnectionState> connState_{ConnectionState::Disconnected};
```

### PlayerController 透传

**新增方法**:

- `bool open(const std::string& url, const NetworkConfig& config)` — 透传给 Demuxer
- `void setConnectionCallback(ConnectionCallback cb)` — 透传给 Demuxer
- `ConnectionState connectionState() const` — 查询

### 影响范围

| 文件 | 改动类型 |
|------|---------|
| `src/core/demuxer/NetworkConfig.h` | 新建 |
| `src/core/demuxer/Demuxer.h` | 新增方法和成员 |
| `src/core/demuxer/Demuxer.cpp` | 重连逻辑实现 |
| `src/core/controller/PlayerController.h` | 新增重载和透传方法 |
| `src/core/controller/PlayerController.cpp` | 透传实现 |

不改动平台层代码，不影响 Android 端。

---

## 模块 2: 硬件解码纹理渲染打通

### 目标

打通 Windows D3D11VA 和 Android MediaCodec 的硬解纹理渲染路径，实现零拷贝或近零拷贝显示。

### NativeTexture 统一结构体

在 `src/core/common/VideoFrame.h` 中新增:

```cpp
struct NativeTexture {
    enum Type { D3D11_TEXTURE, GL_TEXTURE_EXTERNAL_OES };
    Type type = D3D11_TEXTURE;
    void* handle = nullptr;       // ID3D11Texture2D* 或 GLuint 纹理 ID
    int width = 0;
    int height = 0;
    int index = 0;                // D3D11VA 纹理数组索引
};
```

`VideoFrame` 新增字段:
```cpp
NativeTexture nativeTex;   // 当 format == NativeTexture 时有效
```

### IRenderer 接口更新

```cpp
// 替换原 renderTexture(void*, int, int)
virtual bool renderTexture(const NativeTexture& texture) = 0;
```

### Windows: D3D11VA 零拷贝渲染

**共享设备策略**: D3D11VA 解码器和 D3D11 渲染器共享同一个 `ID3D11Device`。

**D3D11Renderer 改造**:

- 新增 `ID3D11Device* getDevice() const` — 暴露设备给解码器
- 实现 `renderTexture(const NativeTexture&)`:
  1. 从 `NativeTexture.handle` 获取源 `ID3D11Texture2D*`
  2. `context_->CopySubresourceRegion()` 将源纹理指定索引拷贝到后台缓冲区
  3. `swapChain_->Present(1, 0)` 呈现

**D3D11VAHardwareDecoder 改造**:

- `init()` 新增 `ID3D11Device* sharedDevice` 参数
- 用共享设备创建 `hw_deviceCtx`（通过 `av_hwdevice_ctx_create_derived` 或直接使用 D3D11VA device context）
- `decode()` 成功后将纹理信息填充到输出结构

**PlayerController 改造**:

- `open()` 中创建硬解时，从 Renderer 获取设备传给 Decoder
- `videoDecodeThread()` 区分软解/硬解帧:
  ```cpp
  if (videoDecoder_->isHardware()) {
      VideoFrame vf;
      vf.format = VideoFrame::NativeTexture;
      vf.nativeTex = ...; // 从解码器获取
      videoFrameQueue_.push(std::move(vf));
  }
  ```

### Android: MediaCodec Surface 输出渲染

**新增 SurfaceTextureRenderer**:

```
src/platform/android/jni/SurfaceTextureRenderer.h
src/platform/android/jni/SurfaceTextureRenderer.cpp
```

职责:
- 创建 `GL_TEXTURE_EXTERNAL_OES` 纹理
- 通过 JNI 创建 `android.graphics.SurfaceTexture` 并绑定到 OES 纹理
- 提供 `Surface` 给 MediaCodec 作为解码输出
- `updateTexImage()` 将最新帧更新到 OES 纹理
- 管理 OES 专用着色器程序

**OES 片元着色器**:

```glsl
#version 300 es
#extension GL_OES_EGL_image_external_essl3 : require
precision mediump float;
uniform samplerExternalOES sTexture;
in vec2 vTexCoord;
out vec4 fragColor;
void main() {
    fragColor = texture(sTexture, vTexCoord);
}
```

**MediaCodecDecoder 改造**:

- 新增 `void setOutputSurface(void* surface)` — 接收 Java Surface 的 jobject
- 解码后调用 `AMediaCodec_releaseOutputBuffer(index, true)` 将帧渲染到 Surface
- 新增 `bool getTextureInfo(NativeTexture& tex)` — 返回当前 OES 纹理信息

**GLESRenderer 改造**:

- 实现 `renderTexture(const NativeTexture&)`:
  1. 绑定 OES 纹理
  2. 使用 OES 着色器程序
  3. 绘制全屏四边形
  4. `eglSwapBuffers()` 呈现

**MPlayerJNI 改造**:

- `init()` 时从 Java SurfaceView 获取 Surface，传给 MediaCodecDecoder

### DecoderFactory Auto 模式完善

确保 `DecoderFactory::createVideoDecoder(DecoderType::Auto)`:
1. Windows: 尝试创建 `D3D11VAHardwareDecoder`，失败则回退 `SoftwareDecoder`
2. Android: 尝试创建 `MediaCodecDecoder`，失败则回退 `SoftwareDecoder`
3. Auto 模式接受 `ID3D11Device*` 可选参数（Windows 平台设备共享）

### 影响范围

| 文件 | 改动类型 |
|------|---------|
| `src/core/common/VideoFrame.h` | 新增 NativeTexture 结构体和字段 |
| `src/core/renderer/IRenderer.h` | renderTexture 参数改为 NativeTexture |
| `src/platform/windows/D3D11Renderer.h/cpp` | 暴露设备 + 实现 renderTexture |
| `src/platform/windows/D3D11VAHardwareDecoder.h/cpp` | 共享设备 + 输出 NativeTexture |
| `src/platform/windows/WinMainWindow.cpp` | 传递设备给解码器 |
| `src/platform/android/GLESRenderer.h/cpp` | OES 着色器 + renderTexture |
| `src/platform/android/MediaCodecDecoder.h/cpp` | Surface 输出改造 |
| `src/platform/android/jni/SurfaceTextureRenderer.h/cpp` | 新建 |
| `src/platform/android/jni/MPlayerJNI.cpp` | Surface 传递 |
| `src/core/controller/PlayerController.h/cpp` | 硬解帧路径 + 设备共享 |
| `src/core/decoder/DecoderFactory.h/cpp` | Auto 模式完善 |
| `src/platform/android/CMakeLists.txt` | 新增源文件 |

---

## 模块 3: 高级功能

### 3.1 倍速播放 — 音视频同步变速

**AudioResampler 封装**:

```
src/core/audio/AudioResampler.h
src/core/audio/AudioResampler.cpp
```

```cpp
class AudioResampler {
public:
    bool init(int sampleRate, int channels, AVSampleFormat format);
    bool setSpeed(float speed);         // 0.5x ~ 4.0x
    float getSpeed() const;
    bool process(const AVFrame* input, std::vector<uint8_t>& output);
    void destroy();
};
```

**FFmpeg 滤镜链**: `abuffer (src) → atempo → abuffersink (sink)`

- `atempo` 范围 [0.5, 100.0]。超出 0.5~4.0 范围的速率通过串联多个 atempo 节点实现
- `setSpeed()` 动态调整 `atempo` 的 `tempo` 参数（无需重建滤镜链）

**PlayerController 改造**:

- 新增成员 `std::unique_ptr<AudioResampler> audioResampler_`
- `open()` 初始化 AudioResampler（有音频流时）
- `setSpeed()`: 同时更新 `speed_` 和 `audioResampler_->setSpeed()`
- `audioDecodeThread()`: 解码后经过 AudioResampler 处理再入队
- `videoDecodeThread()`: 软解帧的 PTS 不做修改，渲染端根据速率调整显示间隔

### 3.2 截图功能

**第三方库**: `stb_image_write.h`（单头文件 PNG 编码）

```
third_party/stb/stb_image_write.h
```

**PlayerController 改造**:

- 新增方法 `bool captureFrame(const std::string& savePath)`
- 新增成员:
  ```cpp
  std::mutex frameMutex_;
  VideoFrame lastFrame_;    // 最新解码帧缓存
  ```
- `videoDecodeThread()` 中每解码一帧加锁更新 `lastFrame_`
- `captureFrame()` 实现:
  1. 加锁拷贝 `lastFrame_`
  2. YUV420P → RGBA: 通过 `sws_scale` 转换
  3. 调用 `stbi_write_png(savePath, w, h, 4, rgbaData, stride)` 保存
  4. NativeTexture（硬解帧）: 返回 false（GPU 回读复杂度高，后续迭代）

### 3.3 视频信息

`getVideoInfo()` 已实现，返回 `VideoInfo{codecName, width, height, duration, bitrate, frameRate}`，无需改动。

### 影响范围

| 文件 | 改动类型 |
|------|---------|
| `src/core/audio/AudioResampler.h/cpp` | 新建 — atempo 滤镜封装 |
| `src/core/controller/PlayerController.h` | 新增 captureFrame、audioResampler_、lastFrame_ |
| `src/core/controller/PlayerController.cpp` | 倍速、截图实现 |
| `src/core/CMakeLists.txt` | 新增 AudioResampler.cpp |
| `third_party/stb/stb_image_write.h` | 新建 — PNG 编码 |
| `third_party/stb/CMakeLists.txt` | 新建（header-only 导入） |

---

## 模块间依赖

```
模块 1 (RTMP) ─── 独立
模块 2 (硬解) ─── 独立
模块 3 (高级)  └─ 依赖模块 2（截图需识别硬解帧类型）
```

实现顺序: 1 → 2 → 3。

## 不在本次范围内

- P6 音视频同步（音频主时钟）
- wxWidgets UI 迁移
- Windows 渲染循环修复和音频回调连接
- 硬解帧截图（GPU → CPU 回读）
- glad 库集成
- ImGui 集成
- 补充 demuxer/decoder 单元测试
