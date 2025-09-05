#include "../include/screen_recorder.h"
#include "../include/screen_ui.h"
#include "../include/audio_capture.h"
#include "../include/webcam_capture.h"
#include <windows.h>
#include <iostream>

// TDD: Temporarily using console entry point for debugging
int main() {
    std::cout << "=== ScreenIT Debug Mode - TDD Verification ===" << std::endl;
    std::cout << "Checking FFmpeg integration..." << std::endl;
    
    // Trigger microphone permission request
    std::cout << "Requesting microphone permissions..." << std::endl;
    
    // Create the screen recorder  
    ScreenRecorder recorder;
    
    // Create and initialize the UI
    ScreenUI ui;
    ui.setScreenRecorder(&recorder);
    
    HINSTANCE hInstance = GetModuleHandle(NULL);
    if (!ui.init(hInstance)) {
        std::cerr << "Failed to initialize the application!" << std::endl;
        return 1;
    }
    
    std::cout << "Application initialized successfully." << std::endl;
    
    // TDD: Test device enumeration
    std::cout << "\n=== Device Enumeration Test ===" << std::endl;
    auto audioDevices = recorder.getAudioDevices();
    std::cout << "Found " << audioDevices.size() << " audio devices:" << std::endl;
    for (const auto& device : audioDevices) {
        std::cout << "- " << device.name << (device.isDefault ? " (default)" : "") << std::endl;
    }
    
    auto webcamDevices = recorder.getWebcamDevices();
    std::cout << "Found " << webcamDevices.size() << " webcam devices:" << std::endl;
    for (const auto& device : webcamDevices) {
        std::cout << "- " << device.name << (device.isDefault ? " (default)" : "") << std::endl;
    }
    
    // Run the UI message loop
    return ui.run();
}

// Original WinMain for production
/*
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
*/ 