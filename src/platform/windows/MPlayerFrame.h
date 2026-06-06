#pragma once

#include <wx/wx.h>
#include <wx/slider.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/menu.h>
#include <wx/statusbr.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>

#include "core/controller/PlayerController.h"
#include "D3D11Renderer.h"
#include "D3D11VAHardwareDecoder.h"
#include "WinAudioOutput.h"
#include "core/decoder/DecoderFactory.h"

#include <memory>

// Forward declarations
class VideoPanel;
class ControlPanel;

class MPlayerFrame : public wxFrame {
public:
    MPlayerFrame();
    ~MPlayerFrame();

    PlayerController* player() { return player_.get(); }
    D3D11Renderer* renderer() { return renderer_; }

    // UI actions (called by ControlPanel and menu)
    void openFile();
    void openRtmp(const wxString& url);
    void togglePlayPause();
    void stopPlayback();
    void seekTo(double seconds);
    void setVolume(float volume);
    void setSpeed(float speed);
    void captureScreenshot();
    void setDecodeMode(DecoderFactory::DecoderType type);

    // State query
    bool isPlaying() const;
    bool isLoopEnabled() const;

private:
    void createMenuBar();
    void createLayout();

    void onTimer(wxTimerEvent& event);
    void onClose(wxCloseEvent& event);
    void onSize(wxSizeEvent& event);

    void onPlayerStateChanged(PlayerController::State state);
    void onConnectionStateChanged(ConnectionState state, const std::string& msg);

    void updateStatusBar();

    std::unique_ptr<PlayerController> player_;
    D3D11Renderer* renderer_ = nullptr; // raw ptr, owned by player_

    VideoPanel* videoPanel_ = nullptr;
    ControlPanel* controlPanel_ = nullptr;

    wxTimer* updateTimer_ = nullptr;

    ConnectionState connectionState_ = ConnectionState::Disconnected;
    std::string lastOpenedUrl_;

    wxDECLARE_EVENT_TABLE();
};
