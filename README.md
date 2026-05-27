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
#    参考 scripts/download_ffmpeg.ps1
# 2. 构建
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Android

```bash
# 1. 下载 FFmpeg Android 预编译库到 third_party/ffmpeg/android/
#    参考 scripts/download_ffmpeg.sh
# 2. 用 Android Studio 打开 src/android-app/ 构建
```

## 项目结构

- `src/core/` - 平台无关核心层 (Demuxer, Decoder, Renderer, Controller)
- `src/platform/windows/` - Windows 平台实现 (D3D11, WASAPI)
- `src/platform/android/` - Android 平台实现 (OpenGL ES, OpenSL ES, JNI)
- `src/android-app/` - Android Java/Kotlin UI
- `tests/` - 单元测试
- `third_party/` - 第三方库
- `docs/` - 设计文档
