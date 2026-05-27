#include "WinMainWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WinMainWindow window;
    if (!window.init(hInstance, nCmdShow)) {
        return 1;
    }

    window.run();
    window.shutdown();
    return 0;
}
