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
