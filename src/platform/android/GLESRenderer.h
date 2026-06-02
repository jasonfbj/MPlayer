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
    bool renderTexture(const NativeTexture& texture) override;
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
