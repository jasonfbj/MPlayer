# MPlayer 跨平台播放器设计文档

## 项目概述

跨平台音视频播放器，支持 Windows 和 Android，基于 FFmpeg 实现本地文件播放和 RTMP 流播放。个人学习项目，目标是深入理解播放管线原理。

## 技术栈

| 组件 | Windows | Android |
|------|---------|---------|
| 核心层 | C++17 | C++17 (NDK) |
| 视频渲染 | D3D11 (HLSL) | OpenGL ES 3.0 (GLSL) |
| 硬件解码 | D3D11VA | MediaCodec (JNI) |
| 软件解码 | FFmpeg sws_scale | FFmpeg sws_scale |
| 音频输出 | WASAPI / XAudio2 | OpenSL ES / Oboe |
| UI | ImGui + Win32 | Java/Kotlin 原生 |
| 构建 | CMake + Visual Studio | CMake + NDK |

## 整体架构

三层架构：UI层（平台相关）→ Player Core（C++平台无关）→ Platform Layer

```
┌──────────────────────────────────────────────────┐
│                 UI Layer (平台相关)                │
│  Windows: ImGui + Win32  │  Android: JNI Bridge  │
├──────────────────────────────────────────────────┤
│              Player Core (C++ 平台无关)            │
│                                                   │
│  ┌────────────┐    ┌───────────────────────────┐  │
│  │  Demuxer   │───>│  Decoder (抽象接口)        │  │
│  │ (统一输入源) │    │  ├── SoftwareDecoder      │  │
│  │            │    │  │   (FFmpeg软解)           │  │
│  │ 本地文件    │    │  ├── HardwareDecoder      │  │
│  │ RTMP流     │    │  │   Win: D3D11VA          │  │
│  │ 未来扩展    │    │  │   Android: MediaCodec   │  │
│  └────────────┘    │  └── DecoderFactory       │  │
│                    └──────────┬────────────────┘  │
│                               │                   │
│                    ┌──────────▼────────────────┐  │
│                    │  Renderer (抽象接口)        │  │
│                    │  ├── D3D11Renderer (Win)   │  │
│                    │  │   软解: YUV→RGB着色器    │  │
│                    │  │   硬解: D3D11纹理渲染    │  │
│                    │  ├── GLESRenderer (Android)│  │
│                    │  │   软解: YUV→RGB着色器    │  │
│                    │  │   硬解: SurfaceTexture   │  │
│                    │  └── RendererFactory       │  │
│                    └───────────────────────────┘  │
│                                                   │
│  ┌────────────┐  ┌────────────┐                  │
│  │AudioOutput │  │PlayerCtrl  │                  │
│  │(平台实现)   │  │(状态管理)   │                  │
│  └────────────┘  └────────────┘                  │
├──────────────────────────────────────────────────┤
│                Platform Layer                     │
│   Window / Audio / Render Context / Threads       │
└──────────────────────────────────────────────────┘
```

## 模块详细设计

### Demuxer（解封装器）

统一处理所有输入源，对上层不区分文件和流。

```cpp
class Demuxer {
public:
    bool open(const std::string& url);    // 支持文件路径和rtmp://
    void close();

    bool readPacket(AVPacket* packet);    // 读取一个packet
    int getVideoStreamIndex() const;
    int getAudioStreamIndex() const;
    AVCodecParameters* getVideoParams() const;
    AVCodecParameters* getAudioParams() const;

    bool seek(int64_t timestamp);         // seek到指定时间点
    bool isStream() const;                // 是否是网络流
};
```

RTMP流通过 `avformat_open_input("rtmp://...")` 打开，需额外配置网络超时、buffer大小等参数。Demuxer内部通过URL scheme自动识别输入类型。

### IDecoder（解码器抽象接口）

两个平台都支持软解和硬解，通过工厂模式自动选择。

```cpp
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

**DecoderFactory 选择逻辑**：
1. 查询当前平台是否支持当前编码格式的硬件解码
2. 支持则创建硬解Decoder，否则回退软解
3. 提供手动切换接口，方便测试对比

**硬件解码方案**：

| 平台 | 硬解实现 | 输出格式 |
|------|---------|---------|
| Windows | D3D11VA | ID3D11Texture2D |
| Android | MediaCodec (JNI) | SurfaceTexture (GL_TEXTURE_EXTERNAL_OES) |

### IRenderer（渲染器抽象接口）

支持两种渲染路径：软解帧(YUV数据)和硬解帧(平台纹理)。

```cpp
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool init(void* nativeWindow) = 0;
    virtual bool renderFrame(const VideoFrame& frame) = 0;       // 软解
    virtual bool renderTexture(const NativeTexture& texture) = 0; // 硬解
    virtual void resize(int width, int height) = 0;
    virtual void destroy() = 0;
};

struct NativeTexture {
    enum Type { D3D11_TEXTURE, GL_TEXTURE };
    Type type;
    union {
        ID3D11Texture2D* d3d11Texture;
        GLuint glTexture;
    };
    int width, height;
};
```

**渲染路径**：

| 路径 | Windows | Android |
|------|---------|---------|
| 软解 | YUV数据 → D3D11着色器(YUV→RGB) → 呈现 | YUV数据 → GLSL着色器(YUV→RGB) → 呈现 |
| 硬解 | D3D11VA纹理 → D3D11直接渲染(零拷贝) | MediaCodec → SurfaceTexture → GLES纹理渲染 |

### IAudioOutput（音频输出抽象接口）

```cpp
class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    virtual bool init(int sampleRate, int channels, AVSampleFormat format) = 0;
    virtual bool play() = 0;
    virtual bool pause() = 0;
    virtual void setVolume(float volume) = 0;  // 0.0 ~ 1.0
    virtual void destroy() = 0;
};
```

| 平台 | 实现 |
|------|------|
| Windows | WASAPI 或 XAudio2 |
| Android | OpenSL ES 或 Oboe |

### PlayerController（播放控制器）

播放控制中枢，管理整体状态。

```cpp
class PlayerController {
public:
    bool open(const std::string& url);
    void close();

    void play();
    void pause();
    void stop();
    void seek(double seconds);
    void setSpeed(float speed);           // 0.5x, 1.0x, 2.0x
    void setVolume(float volume);

    enum State { Idle, Playing, Paused, Stopped };
    State state() const;
    double duration() const;
    double currentPosition() const;

    bool captureFrame(const std::string& savePath);
    VideoInfo getVideoInfo() const;
};
```

### BlockingQueue（线程安全队列）

```cpp
template<typename T>
class BlockingQueue {
public:
    void push(T item);
    bool pop(T& item, int timeoutMs = -1);
    void clear();
    bool isFull() const;
    bool isEmpty() const;
    void setMaxSize(size_t size);  // 背压控制
};
```

### 数据流

```
读取线程                 解码线程                  渲染/音频
┌──────────┐    ┌──────────────┐    ┌──────────────────┐
│Demuxer   │───>│PacketQueue   │───>│VideoDecoder      │
│readPacket│    │(音频packet)   │    │  ↓               │
│          │───>│PacketQueue   │───>│FrameQueue(video) │──> IRenderer
│          │    │(视频packet)   │    │                  │
└──────────┘    └──────────────┘    │AudioDecoder      │
                                    │  ↓               │
                                    │FrameQueue(audio) │──> IAudioOutput
                                    └──────────────────┘
```

## 线程模型

- **主线程**：UI事件处理 + 视频渲染（按帧率驱动）
- **读取线程**：FFmpeg av_read_frame 读取packet放入PacketQueue
- **解码线程**：从PacketQueue取packet解码为frame放入FrameQueue
- **音频线程**：各平台音频回调，从FrameQueue取音频帧输出

本期不实现音视频同步，后续迭代加入。

## 目录结构

```
MPlayer/
├── CMakeLists.txt                    # 根CMake，条件编译各平台
├── scripts/
│   ├── download_ffmpeg.sh            # macOS/Linux下载FFmpeg
│   └── download_ffmpeg.ps1           # Windows下载FFmpeg
│
├── third_party/                      # 第三方库（.gitignore排除二进制）
│   ├── ffmpeg/                       # FFmpeg预编译库
│   │   ├── CMakeLists.txt
│   │   ├── windows/x64/
│   │   │   ├── include/
│   │   │   ├── lib/
│   │   │   └── bin/
│   │   ├── android/
│   │   │   ├── arm64-v8a/{include,lib}/
│   │   │   └── armeabi-v7a/{include,lib}/
│   │   └── macOS/arm64/{include,lib}/
│   ├── glad/                         # OpenGL ES函数加载器
│   └── imgui/                        # ImGui (Windows UI)
│
├── src/
│   ├── core/                         # 平台无关核心层
│   │   ├── CMakeLists.txt
│   │   ├── demuxer/
│   │   │   ├── Demuxer.h
│   │   │   └── Demuxer.cpp
│   │   ├── decoder/
│   │   │   ├── IDecoder.h
│   │   │   ├── SoftwareDecoder.h/cpp
│   │   │   ├── DecoderFactory.h/cpp
│   │   │   └── HardwareDecoder.h/cpp
│   │   ├── renderer/
│   │   │   ├── IRenderer.h
│   │   │   └── RendererFactory.h/cpp
│   │   ├── audio/
│   │   │   ├── IAudioOutput.h
│   │   │   └── AudioFrame.h
│   │   ├── controller/
│   │   │   ├── PlayerController.h
│   │   │   └── PlayerController.cpp
│   │   └── common/
│   │       ├── FrameQueue.h
│   │       ├── PacketQueue.h
│   │       └── NonCopyable.h
│   │
│   ├── platform/                     # 平台相关实现
│   │   ├── windows/
│   │   │   ├── CMakeLists.txt
│   │   │   ├── D3D11Renderer.h/cpp
│   │   │   ├── D3D11VAHardwareDecoder.h/cpp
│   │   │   ├── WinAudioOutput.h/cpp
│   │   │   ├── WinMainWindow.h/cpp
│   │   │   ├── main.cpp
│   │   │   └── shaders/
│   │   │       ├── yuv2rgb.hlsl
│   │   │       └── present.hlsl
│   │   │
│   │   └── android/
│   │       ├── CMakeLists.txt
│   │       ├── GLESRenderer.h/cpp
│   │       ├── MediaCodecDecoder.h/cpp
│   │       ├── AndroidAudioOutput.h/cpp
│   │       ├── shaders/
│   │       │   ├── yuv2rgb.vert
│   │       │   └── yuv2rgb.frag
│   │       └── jni/
│   │           ├── MPlayerJNI.h/cpp
│   │           └── SurfaceTextureRenderer.h/cpp
│   │
│   └── android-app/                  # Android Java/Kotlin UI
│       ├── build.gradle
│       └── app/src/main/
│           ├── java/.../MPlayerActivity.java
│           ├── AndroidManifest.xml
│           └── res/layout/
│
├── tests/
│   ├── CMakeLists.txt
│   ├── test_demuxer.cpp
│   ├── test_decoder.cpp
│   └── test_queue.cpp
│
└── docs/
    └── superpowers/specs/
```

## 依赖管理

### 需要外部管理的库

| 库 | 用途 | 管理方式 |
|----|------|---------|
| FFmpeg | 解封装、解码、格式转换 | 预编译二进制，脚本下载 |
| glad | OpenGL ES函数加载器 | 源码包含 |
| ImGui | Windows UI | 源码包含 |

### 系统自带库（不需要额外下载）

| 平台 | 库 |
|------|-----|
| Windows | D3D11, DXGI, D3DCompiler, WASAPI/XAudio2 (Windows SDK) |
| Android | OpenGL ES 3.0, OpenSL ES/AAudio (Android NDK), MediaCodec (Framework) |

## 构建系统

```cmake
# 根CMakeLists.txt 结构
cmake_minimum_required(VERSION 3.22)
project(MPlayer)

add_subdirectory(src/core)

if(WIN32)
    add_subdirectory(src/platform/windows)
elseif(ANDROID)
    add_subdirectory(src/platform/android)
endif()

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

```bash
# Windows
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Android NDK
cmake -B build-android \
    -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24
cmake --build build-android
```

## 实现路线图

| 阶段 | 内容 | 目标 |
|------|------|------|
| **P0 - 基础框架** | CMake构建、核心接口定义、FFmpeg初始化 | 编译通过，能打开文件 |
| **P1 - 软解播放** | Demuxer + SoftwareDecoder + D3D11/GLES渲染 + 音频输出 | 两平台能播放本地视频 |
| **P2 - 播放控制** | PlayerController + 暂停/继续/seek/进度条/音量 | 完整的播放控制 |
| **P3 - RTMP流** | 网络流参数配置、缓冲策略、连接管理 | 能播放RTMP流 |
| **P4 - 硬件解码** | D3D11VA + MediaCodec + 纹理共享渲染 | 两平台硬件加速 |
| **P5 - 高级功能** | 倍速播放、截图、视频信息显示 | 功能完善 |
| **P6 - 音视频同步** | 音频主时钟同步、多时钟策略 | 播放流畅无卡顿 |

每个阶段独立可验证。
