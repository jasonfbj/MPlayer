#include "MPlayerFrame.h"
#include "VideoPanel.h"
#include "ControlPanel.h"

#include <wx/aboutdlg.h>

wxBEGIN_EVENT_TABLE(MPlayerFrame, wxFrame)
    EVT_TIMER(wxID_ANY, MPlayerFrame::onTimer)
    EVT_CLOSE(MPlayerFrame::onClose)
    EVT_SIZE(MPlayerFrame::onSize)
wxEND_EVENT_TABLE()

enum {
    ID_OPEN_FILE = wxID_HIGHEST + 1,
    ID_OPEN_RTMP,
    ID_SCREENSHOT,
    ID_TIMER_UPDATE,
};

MPlayerFrame::MPlayerFrame()
    : wxFrame(nullptr, wxID_ANY, "MPlayer",
              wxDefaultPosition, wxSize(1280, 800))
{
    SetMinSize(wxSize(800, 600));
    SetBackgroundColour(wxColour(45, 45, 48));

    // Create PlayerController
    player_ = std::make_unique<PlayerController>();

    // Create D3D11 renderer (will be initialized when VideoPanel is ready)
    auto renderer = std::make_unique<D3D11Renderer>();
    renderer_ = renderer.get();
    player_->setRenderer(std::move(renderer));

    // Register device-restored callback so hardware decoder is updated
    // when D3D11 device is recreated after device lost
    renderer_->setDeviceRestoredCallback([this](void* newDevice) {
        // Re-open the current file to reinitialize decoder with new device
        if (!lastOpenedUrl_.empty()) {
            auto state = player_->state();
            player_->close();
            if (player_->open(lastOpenedUrl_)) {
                player_->play();
            }
        }
    });

    // Create audio output
    auto audio = std::make_unique<WinAudioOutput>();
    player_->setAudioOutput(std::move(audio));

    // Register hardware decoder factory
    DecoderFactory::registerHardwareCreator([this](void* sharedDevice) ->
        std::unique_ptr<IDecoder> {
        auto decoder = std::make_unique<D3D11VAHardwareDecoder>();
        if (sharedDevice) {
            auto* dev = static_cast<ID3D11Device*>(sharedDevice);
            decoder->setSharedDevice(dev);
        }
        // Share the renderer's mutex so FFmpeg D3D11VA and renderer
        // synchronize access to the same ID3D11DeviceContext
        if (renderer_) {
            decoder->setContextMutex(&renderer_->contextMutex());
        }
        return decoder;
    });

    // Set callbacks — use CallAfter to marshal to main thread
    player_->setStateCallback([this](PlayerController::State state) {
        CallAfter([this, state]() { onPlayerStateChanged(state); });
    });
    player_->setErrorCallback([this](const std::string& msg) {
        CallAfter([this, msg]() {
            wxMessageBox(wxString::FromUTF8(msg), "Error",
                         wxOK | wxICON_ERROR, this);
        });
    });
    player_->setConnectionCallback(
        [this](ConnectionState state, const std::string& msg) {
            CallAfter([this, state, msg]() {
                onConnectionStateChanged(state, msg);
            });
        });

    createMenuBar();
    createLayout();

    // Create update timer (30ms = ~33fps for UI updates)
    updateTimer_ = new wxTimer(this, ID_TIMER_UPDATE);
    updateTimer_->Start(30);

    CreateStatusBar(3);
    SetStatusText("Ready", 0);
    SetStatusText("", 1);
    SetStatusText("", 2);

    Centre();
}

MPlayerFrame::~MPlayerFrame() {
    // Stop timer first to prevent callbacks during teardown
    if (updateTimer_) {
        updateTimer_->Stop();
        delete updateTimer_;
        updateTimer_ = nullptr;
    }
    // Clear callbacks before destroying player to prevent
    // CallAfter into a half-destroyed frame
    if (player_) {
        player_->setStateCallback(nullptr);
        player_->setErrorCallback(nullptr);
        player_->setConnectionCallback(nullptr);
        player_->close();
        player_.reset();
    }
    renderer_ = nullptr;
}

void MPlayerFrame::createMenuBar() {
    auto* menuBar = new wxMenuBar;

    auto* fileMenu = new wxMenu;
    fileMenu->Append(ID_OPEN_FILE, "Open File...\tCtrl+O");
    fileMenu->Append(ID_OPEN_RTMP, "Open RTMP URL...\tCtrl+R");
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_SCREENSHOT, "Screenshot...\tCtrl+S");
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, "Exit\tAlt+F4");

    menuBar->Append(fileMenu, "&File");

    auto* helpMenu = new wxMenu;
    helpMenu->Append(wxID_ABOUT, "About");
    menuBar->Append(helpMenu, "&Help");

    SetMenuBar(menuBar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { openFile(); }, ID_OPEN_FILE);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxString url = wxGetTextFromUser(
            "Enter RTMP/RTSP/HTTP URL:", "Open Stream",
            "rtmp://", this);
        if (!url.IsEmpty()) {
            openRtmp(url);
        }
    }, ID_OPEN_RTMP);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { captureScreenshot(); },
         ID_SCREENSHOT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(); }, wxID_EXIT);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        wxAboutDialogInfo info;
        info.SetName("MPlayer");
        info.SetVersion("0.1.0");
        info.SetDescription("Cross-platform media player built with C++17, "
                            "FFmpeg, D3D11, and wxWidgets.");
        info.SetCopyright("(C) 2026");
        wxAboutBox(info, this);
    }, wxID_ABOUT);
}

void MPlayerFrame::createLayout() {
    auto* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Video panel takes most space
    videoPanel_ = new VideoPanel(this);
    videoPanel_->SetMinSize(wxSize(640, 360));
    mainSizer->Add(videoPanel_, wxSizerFlags(1).Expand().Border(wxALL, 0));

    // Control panel at bottom
    controlPanel_ = new ControlPanel(this);
    mainSizer->Add(controlPanel_, wxSizerFlags(0).Expand().Border(wxALL, 0));

    SetSizer(mainSizer);
    Layout();
}

void MPlayerFrame::openFile() {
    // Ensure D3D11 is initialized so hardware decoder can share the device
    if (videoPanel_ && !videoPanel_->isD3D11Ready()) {
        if (!videoPanel_->initD3D11()) {
            wxMessageBox("Failed to initialize D3D11 renderer.", "Error",
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }

    wxFileDialog dlg(this, "Open Video File", "", "",
                     "Video Files (*.mp4;*.avi;*.mkv;*.flv;*.mov;*.wmv;*.ts)|"
                     "*.mp4;*.avi;*.mkv;*.flv;*.mov;*.wmv;*.ts|"
                     "All Files (*.*)|*.*",
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() == wxID_OK) {
        player_->close();
        std::string path = dlg.GetPath().ToUTF8().data();
        if (player_->open(path)) {
            lastOpenedUrl_ = path;
            player_->play();
            SetStatusText(wxString::FromUTF8(path), 0);
        } else {
            SetStatusText("Failed to open file", 0);
        }
    }
}

void MPlayerFrame::openRtmp(const wxString& url) {
    if (videoPanel_ && !videoPanel_->isD3D11Ready()) {
        if (!videoPanel_->initD3D11()) {
            wxMessageBox("Failed to initialize D3D11 renderer.", "Error",
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }

    player_->close();
    std::string urlStr = url.ToUTF8().data();
    if (player_->open(urlStr)) {
        lastOpenedUrl_ = urlStr;
        player_->play();
        SetStatusText(url, 0);
    } else {
        SetStatusText("Failed to connect", 0);
    }
}

void MPlayerFrame::togglePlayPause() {
    if (!player_) return;
    auto state = player_->state();
    if (state == PlayerController::Playing) {
        player_->pause();
    } else if (state == PlayerController::Paused) {
        player_->play();
    } else if (state == PlayerController::Stopped || state == PlayerController::Idle) {
        player_->play();
    }
}

void MPlayerFrame::stopPlayback() {
    if (player_) player_->stop();
}

void MPlayerFrame::seekTo(double seconds) {
    if (player_) player_->seek(seconds);
}

void MPlayerFrame::setVolume(float volume) {
    if (player_) player_->setVolume(volume);
}

void MPlayerFrame::setSpeed(float speed) {
    if (player_) player_->setSpeed(speed);
}

void MPlayerFrame::setDecodeMode(DecoderFactory::DecoderType type) {
    if (!player_) return;
    if (player_->decoderType() == type) return;

    // Save current state for reopen
    auto prevState = player_->state();
    double pos = player_->currentPosition();

    player_->setDecoderType(type);

    // Reopen current file with new decoder type if something was loaded
    if (prevState != PlayerController::Idle && !lastOpenedUrl_.empty()) {
        player_->close();
        if (player_->open(lastOpenedUrl_)) {
            player_->play();
            // Seek back to previous position
            if (pos > 0) {
                player_->seek(pos);
            }
        } else {
            // Hardware decode failed — fall back to software so the user can still play
            player_->setDecoderType(DecoderFactory::DecoderType::Software);
            if (player_->open(lastOpenedUrl_)) {
                player_->play();
                if (pos > 0) {
                    player_->seek(pos);
                }
                // Sync the UI dropdown to reflect the actual (Software) mode
                if (controlPanel_) {
                    controlPanel_->setDecodeModeSelection(1);  // 1 = Software
                }
                wxMessageBox("Hardware decode failed for this video. "
                             "Switched to Software decode.",
                             "Decode Mode", wxOK | wxICON_INFORMATION, this);
            }
        }
    }
}

void MPlayerFrame::captureScreenshot() {
    if (!player_) return;
    wxFileDialog dlg(this, "Save Screenshot", "", "screenshot.png",
                     "PNG Files (*.png)|*.png",
                     wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() == wxID_OK) {
        std::string path = dlg.GetPath().ToUTF8().data();
        if (player_->captureFrame(path)) {
            wxMessageBox("Screenshot saved!", "Success",
                         wxOK | wxICON_INFORMATION, this);
        } else {
            wxMessageBox("Failed to capture screenshot.", "Error",
                         wxOK | wxICON_ERROR, this);
        }
    }
}

bool MPlayerFrame::isPlaying() const {
    return player_ && player_->state() == PlayerController::Playing;
}

bool MPlayerFrame::isLoopEnabled() const {
    return controlPanel_ && controlPanel_->isLoopEnabled();
}

void MPlayerFrame::onTimer(wxTimerEvent&) {
    // Drive video rendering
    if (videoPanel_) {
        videoPanel_->renderFrame();
    }

    // Update UI controls
    if (controlPanel_) {
        controlPanel_->updateUI();
    }

    updateStatusBar();

    // Loop playback: when playback is complete (EOF + all frames consumed)
    if (player_ && player_->state() == PlayerController::Playing &&
        player_->isPlaybackComplete()) {
        if (isLoopEnabled() && !lastOpenedUrl_.empty()) {
            player_->close();
            if (player_->open(lastOpenedUrl_)) {
                player_->play();
            }
        } else {
            player_->stop();
        }
    }
}

void MPlayerFrame::onPlayerStateChanged(PlayerController::State state) {
    if (controlPanel_) {
        controlPanel_->onPlayerStateChanged(state);
    }

    switch (state) {
    case PlayerController::Playing:
        SetStatusText("Playing", 1);
        break;
    case PlayerController::Paused:
        SetStatusText("Paused", 1);
        break;
    case PlayerController::Stopped:
        SetStatusText("Stopped", 1);
        break;
    case PlayerController::Idle:
        SetStatusText("Ready", 1);
        break;
    }
}

void MPlayerFrame::onClose(wxCloseEvent& event) {
    if (updateTimer_) {
        updateTimer_->Stop();
    }
    if (player_) {
        player_->close();
    }
    event.Skip();
}

void MPlayerFrame::onSize(wxSizeEvent& event) {
    event.Skip();
}

void MPlayerFrame::onConnectionStateChanged(ConnectionState state,
                                             const std::string& msg) {
    connectionState_ = state;
    wxString stateText;
    switch (state) {
    case ConnectionState::Disconnected:
        stateText = "Disconnected";
        break;
    case ConnectionState::Connecting:
        stateText = "Connecting...";
        break;
    case ConnectionState::Connected:
        stateText = "Connected";
        break;
    case ConnectionState::Reconnecting:
        stateText = "Reconnecting...";
        break;
    }
    if (!msg.empty()) {
        stateText += " - " + wxString::FromUTF8(msg);
    }
    SetStatusText(stateText, 2);
}

void MPlayerFrame::updateStatusBar() {
    if (!player_ || player_->state() == PlayerController::Idle) return;

    double pos = player_->currentPosition();
    double dur = player_->duration();
    int posMin = static_cast<int>(pos) / 60;
    int posSec = static_cast<int>(pos) % 60;
    int durMin = static_cast<int>(dur) / 60;
    int durSec = static_cast<int>(dur) % 60;

    auto info = player_->getVideoInfo();
    if (dur > 0) {
        wxString timeStr = wxString::Format("%02d:%02d / %02d:%02d",
                                            posMin, posSec, durMin, durSec);
        SetStatusText(timeStr, 1);
    }

    if (!info.codecName.empty()) {
        wxString hwTag = player_->isHardwareDecoding() ? "[HW]" : "[SW]";
        wxString infoStr = wxString::Format("%s %dx%d %.1ffps %s",
                                            wxString::FromUTF8(info.codecName),
                                            info.width, info.height,
                                            info.frameRate, hwTag);
        SetStatusText(infoStr, 2);
    }
}
