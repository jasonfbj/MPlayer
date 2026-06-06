#include "SeekSlider.h"

#ifdef _WIN32
#include <windowsx.h>
#include <commctrl.h>
#endif

wxBEGIN_EVENT_TABLE(SeekSlider, wxSlider)
    EVT_LEFT_DOWN(SeekSlider::onMouseDown)
    EVT_LEFT_UP(SeekSlider::onMouseUp)
    EVT_MOTION(SeekSlider::onMouseMove)
    EVT_MOUSE_CAPTURE_LOST(SeekSlider::onCaptureLost)
wxEND_EVENT_TABLE()

SeekSlider::SeekSlider(wxWindow* parent, wxWindowID id, int value,
                       int minValue, int maxValue,
                       const wxPoint& pos, const wxSize& size,
                       long style, const wxValidator& validator,
                       const wxString& name)
    : wxSlider(parent, id, value, minValue, maxValue, pos, size, style, validator, name)
{
    // Mouse events are registered via the event table above — no need for Bind().
}

int SeekSlider::posFromMouseX(int mouseX) const {
    // Get the native TrackBar channel rectangle to compute precise position.
    // The channel is the grooved area between the two end padding zones.
    int min = GetMin();
    int max = GetMax();
    if (max <= min) return min;

#ifdef _WIN32
    HWND hwnd = GetHWND();
    RECT channelRect;
    // TBM_GETCHANNELRECT retrieves the bounding rectangle of the trackbar's channel
    SendMessage(hwnd, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channelRect));

    int channelLeft = channelRect.left;
    int channelRight = channelRect.right;
    int channelWidth = channelRight - channelLeft;

    if (channelWidth <= 0) {
        // Fallback: use full client area
        RECT clientRect;
        ::GetClientRect(hwnd, &clientRect);
        channelLeft = clientRect.left;
        channelRight = clientRect.right;
        channelWidth = channelRight - channelLeft;
    }

    if (channelWidth <= 0) return min;

    // Clamp mouseX to channel bounds
    int clampedX = std::max(channelLeft, std::min(mouseX, channelRight));

    // Linear interpolation: channelLeft → min, channelRight → max
    double ratio = static_cast<double>(clampedX - channelLeft) / channelWidth;
    int value = min + static_cast<int>(ratio * (max - min));
    return std::max(min, std::min(max, value));
#else
    // Non-Windows fallback: use client width
    wxSize clientSize = GetClientSize();
    int width = clientSize.GetWidth();
    if (width <= 0) return min;
    int clampedX = std::max(0, std::min(mouseX, width));
    double ratio = static_cast<double>(clampedX) / width;
    return min + static_cast<int>(ratio * (max - min));
#endif
}

void SeekSlider::onMouseDown(wxMouseEvent& event) {
#ifdef _WIN32
    HWND hwnd = GetHWND();
    POINT pt;
    GetCursorPos(&pt);
    ::ScreenToClient(hwnd, &pt);

    // Determine if the click is on the thumb or elsewhere on the channel
    RECT thumbRect;
    SendMessage(hwnd, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumbRect));

    bool onThumb = PtInRect(&thumbRect, pt) != 0;

    if (!onThumb) {
        // Click on the channel — compute exact position and jump there
        int newPos = posFromMouseX(pt.x);
        SetValue(newPos);
        clickSeeking_ = true;

        // Fire a scroll event so the parent knows the value changed
        wxScrollEvent evt(wxEVT_SCROLL_CHANGED, GetId());
        evt.SetPosition(newPos);
        evt.SetEventObject(this);
        ProcessWindowEvent(evt);

        // Capture mouse for drag-to-seek after clicking the channel
        CaptureMouse();
        dragging_ = true;
        return;  // Don't skip — we've handled the click
    }
#endif

    // Click on the thumb — let default behavior handle it (normal thumb dragging)
    dragging_ = false;
    event.Skip();
}

void SeekSlider::onMouseUp(wxMouseEvent& event) {
    if (dragging_) {
        // Release mouse capture from channel-click drag
        if (HasCapture()) {
            ReleaseMouse();
        }
        dragging_ = false;
        clickSeeking_ = false;

        // Fire the final commit event
        int pos = GetValue();
        wxCommandEvent evt(wxEVT_SLIDER, GetId());
        evt.SetInt(pos);
        evt.SetEventObject(this);
        ProcessWindowEvent(evt);
        return;
    }

    clickSeeking_ = false;
    event.Skip();
}

void SeekSlider::onMouseMove(wxMouseEvent& event) {
    if (dragging_ && clickSeeking_) {
        // Dragging after channel click — update slider position in real-time
#ifdef _WIN32
        HWND hwnd = GetHWND();
        POINT pt;
        GetCursorPos(&pt);
        ::ScreenToClient(hwnd, &pt);

        int newPos = posFromMouseX(pt.x);
        SetValue(newPos);

        // Fire drag event for live preview (time label update)
        wxScrollEvent evt(wxEVT_SCROLL_THUMBTRACK, GetId());
        evt.SetPosition(newPos);
        evt.SetEventObject(this);
        ProcessWindowEvent(evt);
        return;
#endif
    }

    event.Skip();
}

void SeekSlider::onCaptureLost(wxMouseCaptureLostEvent& event) {
    if (dragging_ || clickSeeking_) {
        dragging_ = false;
        clickSeeking_ = false;

        // Fire commit event so ControlPanel resets seeking_ and performs the seek
        int pos = GetValue();
        wxCommandEvent evt(wxEVT_SLIDER, GetId());
        evt.SetInt(pos);
        evt.SetEventObject(this);
        ProcessWindowEvent(evt);
    }
    event.Skip();
}
