#pragma once

#include <wx/wx.h>
#include <d3d11.h>
#include <wrl/client.h>

class MPlayerFrame;

class VideoPanel : public wxPanel {
public:
    VideoPanel(MPlayerFrame* frame);

    // Called by the timer to render one frame
    void renderFrame();

    // Initialize D3D11 with this panel's HWND
    bool initD3D11();

    // Check if D3D11 is initialized and ready
    bool isD3D11Ready() const { return d3d11Ready_; }

    HWND getHwnd() const { return GetHWND(); }

private:
    void onPaint(wxPaintEvent& event);
    void onEraseBackground(wxEraseEvent& event);
    void onSize(wxSizeEvent& event);

    MPlayerFrame* frame_ = nullptr;
    bool d3d11Ready_ = false;

    wxDECLARE_EVENT_TABLE();
};
