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

    void renderUI();
    void handleFileOpen();
    void handleRtmpInput();

    HWND hwnd_ = nullptr;
    std::unique_ptr<PlayerController> player_;
    int windowWidth_ = 1280;
    int windowHeight_ = 720;
    bool running_ = false;

    char filePath_[512] = "";
    char rtmpUrl_[512] = "";
};
