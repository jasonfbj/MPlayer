#pragma once

#include <wx/wx.h>

class MPlayerApp : public wxApp {
public:
    bool OnInit() override;
    int OnExit() override;
};
