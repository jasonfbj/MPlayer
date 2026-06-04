#include "VideoPanel.h"
#include "MPlayerFrame.h"

wxBEGIN_EVENT_TABLE(VideoPanel, wxPanel)
    EVT_PAINT(VideoPanel::onPaint)
    EVT_ERASE_BACKGROUND(VideoPanel::onEraseBackground)
    EVT_SIZE(VideoPanel::onSize)
wxEND_EVENT_TABLE()

VideoPanel::VideoPanel(MPlayerFrame* frame)
    : wxPanel(frame, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxNO_FULL_REPAINT_ON_RESIZE | wxCLIP_CHILDREN),
      frame_(frame)
{
    SetBackgroundColour(wxColour(0, 0, 0));
    SetMinSize(wxSize(320, 180));

    // Initialize D3D11 immediately — the HWND is available
    // after the base wxPanel constructor completes.
    // This MUST happen before any file is opened so that
    // the D3D11 device is available for hardware decoder sharing.
    initD3D11();
}

bool VideoPanel::initD3D11() {
    if (d3d11Ready_) return true;

    auto* renderer = frame_->renderer();
    if (!renderer) return false;

    HWND hwnd = GetHWND();
    if (!hwnd) return false;

    if (!renderer->init(hwnd)) {
        return false;
    }

    d3d11Ready_ = true;
    return true;
}

void VideoPanel::renderFrame() {
    auto* player = frame_->player();
    auto* renderer = frame_->renderer();
    if (!player || !renderer || !d3d11Ready_) return;

    if (player->state() != PlayerController::Playing &&
        player->state() != PlayerController::Paused) {
        return;
    }

    // Consume all available frames, keep only the latest
    VideoFrame latestFrame;
    bool hasFrame = false;

    VideoFrame frame;
    while (player->videoFrameQueue().pop(frame, 0)) {
        latestFrame = std::move(frame);
        hasFrame = true;
    }

    if (hasFrame) {
        if (latestFrame.format == VideoFrame::NativeTexture) {
            renderer->renderTexture(latestFrame.nativeTex);
        } else {
            renderer->renderFrame(latestFrame);
        }
    }
}

void VideoPanel::onPaint(wxPaintEvent&) {
    wxPaintDC dc(this);
    renderFrame();
}

void VideoPanel::onEraseBackground(wxEraseEvent&) {
    // Prevent flicker — D3D11 handles the background
}

void VideoPanel::onSize(wxSizeEvent& event) {
    event.Skip();

    auto* renderer = frame_->renderer();
    if (renderer && d3d11Ready_) {
        wxSize size = GetClientSize();
        if (size.GetWidth() > 0 && size.GetHeight() > 0) {
            renderer->resize(size.GetWidth(), size.GetHeight());
        }
    }
}
