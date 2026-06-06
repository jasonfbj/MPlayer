#pragma once

#include <wx/wx.h>
#include <wx/slider.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/bmpbuttn.h>
#include <wx/artprov.h>
#include <wx/checkbox.h>
#include <wx/choice.h>

#include "core/controller/PlayerController.h"
#include "SeekSlider.h"

class MPlayerFrame;

class ControlPanel : public wxPanel {
public:
    ControlPanel(MPlayerFrame* frame);

    // Called by the timer to update seek bar and time labels
    void updateUI();

    // Called when player state changes
    void onPlayerStateChanged(PlayerController::State state);

    // Loop mode query
    bool isLoopEnabled() const { return loopCheckbox_->GetValue(); }

    // Sync UI dropdown with actual decoder type (e.g., after HW→SW fallback)
    void setDecodeModeSelection(int index) { decodeModeChoice_->SetSelection(index); }

private:
    void createControls();

    void onPlayPause(wxCommandEvent& event);
    void onStop(wxCommandEvent& event);
    void onSeekDrag(wxScrollEvent& event);
    void onSeekCommit(wxCommandEvent& event);
    void onVolumeChange(wxScrollEvent& event);
    void onSpeedChange(wxCommandEvent& event);
    void onRtmpConnect(wxCommandEvent& event);
    void onFileOpen(wxCommandEvent& event);
    void onDecodeModeChange(wxCommandEvent& event);

    MPlayerFrame* frame_ = nullptr;

    // Controls
    wxButton* playPauseBtn_ = nullptr;
    wxButton* stopBtn_ = nullptr;
    wxButton* openFileBtn_ = nullptr;
    SeekSlider* seekSlider_ = nullptr;
    wxStaticText* timeLabel_ = nullptr;
    wxSlider* volumeSlider_ = nullptr;
    wxStaticText* volumeLabel_ = nullptr;
    wxComboBox* speedCombo_ = nullptr;
    wxTextCtrl* rtmpInput_ = nullptr;
    wxButton* rtmpConnectBtn_ = nullptr;
    wxStaticText* videoInfoLabel_ = nullptr;
    wxStaticText* connectionLabel_ = nullptr;
    wxCheckBox* loopCheckbox_ = nullptr;
    wxChoice* decodeModeChoice_ = nullptr;

    bool seeking_ = false;

    wxDECLARE_EVENT_TABLE();
};
