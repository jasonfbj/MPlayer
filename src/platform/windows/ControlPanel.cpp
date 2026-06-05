#include "ControlPanel.h"
#include "MPlayerFrame.h"

enum {
    ID_PLAY_PAUSE = wxID_HIGHEST + 100,
    ID_STOP,
    ID_OPEN_FILE_BTN,
    ID_SEEK_SLIDER,
    ID_VOLUME_SLIDER,
    ID_SPEED_COMBO,
    ID_RTMP_INPUT,
    ID_RTMP_CONNECT,
    ID_LOOP_CHECKBOX,
};

wxBEGIN_EVENT_TABLE(ControlPanel, wxPanel)
    EVT_BUTTON(ID_PLAY_PAUSE, ControlPanel::onPlayPause)
    EVT_BUTTON(ID_STOP, ControlPanel::onStop)
    EVT_BUTTON(ID_OPEN_FILE_BTN, ControlPanel::onFileOpen)
    EVT_COMMAND_SCROLL(ID_SEEK_SLIDER, ControlPanel::onSeek)
    EVT_COMMAND_SCROLL_THUMBRELEASE(ID_SEEK_SLIDER,
                                     ControlPanel::onSeekRelease)
    EVT_COMMAND_SCROLL(ID_VOLUME_SLIDER, ControlPanel::onVolumeChange)
    EVT_COMBOBOX(ID_SPEED_COMBO, ControlPanel::onSpeedChange)
    EVT_TEXT_ENTER(ID_RTMP_INPUT, ControlPanel::onRtmpConnect)
    EVT_BUTTON(ID_RTMP_CONNECT, ControlPanel::onRtmpConnect)
wxEND_EVENT_TABLE()

ControlPanel::ControlPanel(MPlayerFrame* frame)
    : wxPanel(frame, wxID_ANY), frame_(frame)
{
    SetBackgroundColour(wxColour(50, 50, 54));
    createControls();
}

void ControlPanel::createControls() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // -- Row 1: Seek bar + time labels --
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);

        timeLabel_ = new wxStaticText(this, wxID_ANY, "00:00 / 00:00");
        timeLabel_->SetForegroundColour(wxColour(220, 220, 220));
        timeLabel_->SetMinSize(wxSize(120, -1));
        row->Add(timeLabel_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

        seekSlider_ = new wxSlider(this, ID_SEEK_SLIDER, 0, 0, 10000,
                                    wxDefaultPosition, wxDefaultSize,
                                    wxSL_HORIZONTAL | wxSL_TICKS);
        row->Add(seekSlider_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);

        mainSizer->Add(row, 0, wxEXPAND | wxTOP | wxLEFT | wxRIGHT, 6);
    }

    // -- Row 2: Playback controls --
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);

        // Open File button
        openFileBtn_ = new wxButton(this, ID_OPEN_FILE_BTN, "Open");
        openFileBtn_->SetToolTip("Open a video file (Ctrl+O)");
        row->Add(openFileBtn_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);
        row->AddSpacer(6);

        // Play/Pause button
        playPauseBtn_ = new wxButton(this, ID_PLAY_PAUSE, "Play");
        playPauseBtn_->SetMinSize(wxSize(80, 32));
        playPauseBtn_->SetToolTip("Play / Pause");
        row->Add(playPauseBtn_, 0, wxALIGN_CENTER_VERTICAL);
        row->AddSpacer(4);

        // Stop button
        stopBtn_ = new wxButton(this, ID_STOP, "Stop");
        stopBtn_->SetMinSize(wxSize(70, 32));
        stopBtn_->SetToolTip("Stop playback");
        row->Add(stopBtn_, 0, wxALIGN_CENTER_VERTICAL);
        row->AddSpacer(4);

        // Loop checkbox
        loopCheckbox_ = new wxCheckBox(this, ID_LOOP_CHECKBOX, "Loop");
        loopCheckbox_->SetForegroundColour(wxColour(220, 220, 220));
        loopCheckbox_->SetToolTip("Repeat playback when finished");
        loopCheckbox_->SetValue(false);
        row->Add(loopCheckbox_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
        row->AddSpacer(12);

        // Volume label + slider
        auto* volLabel = new wxStaticText(this, wxID_ANY, "Vol:");
        volLabel->SetForegroundColour(wxColour(220, 220, 220));
        row->Add(volLabel, 0, wxALIGN_CENTER_VERTICAL);

        volumeSlider_ = new wxSlider(this, ID_VOLUME_SLIDER, 100, 0, 100,
                                      wxDefaultPosition, wxSize(100, -1),
                                      wxSL_HORIZONTAL);
        volumeSlider_->SetToolTip("Volume");
        row->Add(volumeSlider_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

        volumeLabel_ = new wxStaticText(this, wxID_ANY, "100%");
        volumeLabel_->SetForegroundColour(wxColour(180, 180, 180));
        volumeLabel_->SetMinSize(wxSize(40, -1));
        row->Add(volumeLabel_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
        row->AddSpacer(12);

        // Speed selector
        auto* speedLabel = new wxStaticText(this, wxID_ANY, "Speed:");
        speedLabel->SetForegroundColour(wxColour(180, 180, 180));
        row->Add(speedLabel, 0, wxALIGN_CENTER_VERTICAL);

        wxString speeds[] = {"0.5x", "0.75x", "1.0x", "1.25x", "1.5x", "2.0x"};
        speedCombo_ = new wxComboBox(this, ID_SPEED_COMBO, "1.0x",
                                      wxDefaultPosition, wxSize(80, -1),
                                      WXSIZEOF(speeds), speeds,
                                      wxCB_DROPDOWN | wxTE_PROCESS_ENTER);
        speedCombo_->SetToolTip("Playback speed");
        row->Add(speedCombo_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

        row->AddStretchSpacer();

        // Video info
        videoInfoLabel_ = new wxStaticText(this, wxID_ANY, "");
        videoInfoLabel_->SetForegroundColour(wxColour(160, 200, 255));
        videoInfoLabel_->SetMinSize(wxSize(200, -1));
        row->Add(videoInfoLabel_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        mainSizer->Add(row, 0, wxEXPAND | wxTOP | wxBOTTOM, 4);
    }

    // -- Row 3: RTMP stream input --
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);

        auto* rtmpLabel = new wxStaticText(this, wxID_ANY, "URL:");
        rtmpLabel->SetForegroundColour(wxColour(180, 180, 180));
        row->Add(rtmpLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

        rtmpInput_ = new wxTextCtrl(this, ID_RTMP_INPUT, "",
                                     wxDefaultPosition, wxDefaultSize,
                                     wxTE_PROCESS_ENTER);
        rtmpInput_->SetHint("rtmp:// or rtsp:// or http://");
        row->Add(rtmpInput_, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

        rtmpConnectBtn_ = new wxButton(this, ID_RTMP_CONNECT, "Connect");
        rtmpConnectBtn_->SetMinSize(wxSize(80, 28));
        row->Add(rtmpConnectBtn_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);

        connectionLabel_ = new wxStaticText(this, wxID_ANY, "");
        connectionLabel_->SetForegroundColour(wxColour(255, 200, 100));
        connectionLabel_->SetMinSize(wxSize(100, -1));
        row->Add(connectionLabel_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 8);

        mainSizer->Add(row, 0, wxEXPAND | wxBOTTOM | wxLEFT | wxRIGHT, 6);
    }

    SetSizer(mainSizer);
}

void ControlPanel::onPlayPause(wxCommandEvent&) {
    frame_->togglePlayPause();
}

void ControlPanel::onStop(wxCommandEvent&) {
    frame_->stopPlayback();
}

void ControlPanel::onFileOpen(wxCommandEvent&) {
    frame_->openFile();
}

void ControlPanel::onSeek(wxScrollEvent&) {
    seeking_ = true;
}

void ControlPanel::onSeekRelease(wxScrollEvent&) {
    if (!seeking_) return;
    seeking_ = false;

    int pos = seekSlider_->GetValue();
    int range = seekSlider_->GetMax();
    if (range <= 0) return;

    auto* player = frame_->player();
    if (player && player->duration() > 0) {
        double seekTarget = (static_cast<double>(pos) / range) *
                            player->duration();
        frame_->seekTo(seekTarget);
    }
}

void ControlPanel::onVolumeChange(wxScrollEvent&) {
    int vol = volumeSlider_->GetValue();
    frame_->setVolume(static_cast<float>(vol) / 100.0f);
    volumeLabel_->SetLabel(wxString::Format("%d%%", vol));
}

void ControlPanel::onSpeedChange(wxCommandEvent&) {
    wxString val = speedCombo_->GetValue();
    double speed = 1.0;
    // Parse "1.0x" format
    val.BeforeFirst('x').ToDouble(&speed);
    if (speed < 0.25) speed = 0.25;
    if (speed > 4.0) speed = 4.0;
    frame_->setSpeed(static_cast<float>(speed));
}

void ControlPanel::onRtmpConnect(wxCommandEvent&) {
    wxString url = rtmpInput_->GetValue().Trim().Trim(false);
    if (!url.IsEmpty()) {
        frame_->openRtmp(url);
    }
}

void ControlPanel::updateUI() {
    auto* player = frame_->player();
    if (!player) return;

    // Skip expensive updates when idle
    auto state = player->state();
    if (state == PlayerController::Idle) return;

    // Update seek bar (only if user isn't dragging it)
    double dur = player->duration();
    double pos = player->currentPosition();
    if (dur > 0 && !seeking_) {
        int sliderPos = static_cast<int>((pos / dur) * 10000);
        sliderPos = std::max(0, std::min(10000, sliderPos));
        seekSlider_->SetValue(sliderPos);
    }

    // Update time label
    int posMin = static_cast<int>(pos) / 60;
    int posSec = static_cast<int>(pos) % 60;
    if (dur > 0) {
        int durMin = static_cast<int>(dur) / 60;
        int durSec = static_cast<int>(dur) % 60;
        timeLabel_->SetLabel(wxString::Format("%02d:%02d / %02d:%02d",
                                               posMin, posSec,
                                               durMin, durSec));
    } else {
        timeLabel_->SetLabel(wxString::Format("%02d:%02d / --:--",
                                               posMin, posSec));
    }

    // Update connection state (only for network streams)
    auto connState = player->connectionState();
    switch (connState) {
    case ConnectionState::Disconnected:
        connectionLabel_->SetLabel("");
        connectionLabel_->SetForegroundColour(wxColour(255, 200, 100));
        break;
    case ConnectionState::Connecting:
        connectionLabel_->SetLabel("Connecting...");
        connectionLabel_->SetForegroundColour(wxColour(255, 200, 100));
        break;
    case ConnectionState::Connected:
        connectionLabel_->SetLabel("Connected");
        connectionLabel_->SetForegroundColour(wxColour(100, 255, 100));
        break;
    case ConnectionState::Reconnecting:
        connectionLabel_->SetLabel("Reconnecting...");
        connectionLabel_->SetForegroundColour(wxColour(255, 150, 50));
        break;
    }
}

void ControlPanel::onPlayerStateChanged(PlayerController::State state) {
    switch (state) {
    case PlayerController::Playing:
        playPauseBtn_->SetLabel("Pause");
        break;
    case PlayerController::Paused:
        playPauseBtn_->SetLabel("Play");
        break;
    case PlayerController::Stopped:
    case PlayerController::Idle:
        playPauseBtn_->SetLabel("Play");
        seekSlider_->SetValue(0);
        timeLabel_->SetLabel("00:00 / 00:00");
        videoInfoLabel_->SetLabel("");
        connectionLabel_->SetLabel("");
        break;
    }
}
