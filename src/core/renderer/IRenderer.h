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

    // Return platform native device (Windows: ID3D11Device*, Android: nullptr)
    virtual void* getNativeDevice() const { return nullptr; }
    virtual void* getNativeDeviceContext() const { return nullptr; }
};
