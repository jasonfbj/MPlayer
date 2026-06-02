#include "WinMainWindow.h"
#include "D3D11Renderer.h"
#include "D3D11VAHardwareDecoder.h"
#include "WinAudioOutput.h"
#include "core/decoder/DecoderFactory.h"

#include <commdlg.h>

WinMainWindow::~WinMainWindow() {
    shutdown();
}

bool WinMainWindow::init(HINSTANCE hInstance, int nCmdShow) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MPlayer";
    RegisterClassExW(&wc);

    hwnd_ = CreateWindowExW(0, L"MPlayer", L"MPlayer",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth_, windowHeight_, nullptr, nullptr, hInstance, this);

    if (!hwnd_) return false;

    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);

    player_ = std::make_unique<PlayerController>();

    auto renderer = std::make_unique<D3D11Renderer>();
    if (!renderer->init(hwnd_)) return false;
    player_->setRenderer(std::move(renderer));

    auto audio = std::make_unique<WinAudioOutput>();
    player_->setAudioOutput(std::move(audio));

    // Register hardware decoder factory
    DecoderFactory::registerHardwareCreator([](void* sharedDevice) ->
        std::unique_ptr<IDecoder> {
        auto decoder = std::make_unique<D3D11VAHardwareDecoder>();
        if (sharedDevice) {
            auto* dev = static_cast<ID3D11Device*>(sharedDevice);
            decoder->setSharedDevice(dev);
        }
        return decoder;
    });

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

        if (player_ && player_->state() == PlayerController::Playing) {
            VideoFrame frame;
            while (player_->videoFrameQueue().pop(frame, 0)) {
                // 渲染帧 (通过 renderer)
            }
        }

        // 简单的消息循环，实际项目中用 ImGui 渲染控制面板
    }
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

    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

LRESULT WINAPI WinMainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
