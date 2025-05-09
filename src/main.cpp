#include "../include/screen_recorder.h"
#include "../include/screen_ui.h"
#include <windows.h>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create the screen recorder
    ScreenRecorder recorder;
    
    // Create and initialize the UI
    ScreenUI ui;
    ui.setScreenRecorder(&recorder);
    
    if (!ui.init(hInstance)) {
        MessageBox(NULL, L"Failed to initialize the application!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Run the UI message loop
    return ui.run();
} 