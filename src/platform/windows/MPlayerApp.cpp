#include "MPlayerApp.h"
#include "MPlayerFrame.h"

bool MPlayerApp::OnInit() {
    if (!wxApp::OnInit()) return false;

    auto* frame = new MPlayerFrame();
    frame->Show(true);
    return true;
}

int MPlayerApp::OnExit() {
    return wxApp::OnExit();
}
