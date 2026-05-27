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
