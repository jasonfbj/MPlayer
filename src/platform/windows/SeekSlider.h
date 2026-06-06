#pragma once

#include <wx/wx.h>
#include <wx/slider.h>

// Custom wxSlider subclass that provides precise click-to-seek behavior.
// The native Windows TrackBar control does not jump the thumb to the exact
// click position — it moves by a "page step" instead. This subclass
// intercepts WM_LBUTTONDOWN and manually computes the exact slider value
// from the mouse X coordinate, giving frame-accurate seek bar clicks.
class SeekSlider : public wxSlider {
public:
    SeekSlider(wxWindow* parent, wxWindowID id, int value, int minValue, int maxValue,
               const wxPoint& pos = wxDefaultPosition,
               const wxSize& size = wxDefaultSize,
               long style = wxSL_HORIZONTAL,
               const wxValidator& validator = wxDefaultValidator,
               const wxString& name = wxSliderNameStr);

private:
    // Intercept mouse events for precise click-to-position
    void onMouseDown(wxMouseEvent& event);
    void onMouseUp(wxMouseEvent& event);
    void onMouseMove(wxMouseEvent& event);
    void onCaptureLost(wxMouseCaptureLostEvent& event);

    // Compute slider value from mouse X position
    int posFromMouseX(int mouseX) const;

    bool clickSeeking_ = false;
    bool dragging_ = false;

    wxDECLARE_EVENT_TABLE();
};
