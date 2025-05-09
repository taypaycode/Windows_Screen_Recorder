#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include "screen_recorder.h"

// Window size constants
#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 300

// Custom window messages
#define WM_TRAYICON (WM_USER + 1)
#define WM_START_RECORDING (WM_USER + 2)
#define WM_STOP_RECORDING (WM_USER + 3)

class ScreenUI {
public:
    ScreenUI();
    ~ScreenUI();

    // Initialize the UI
    bool init(HINSTANCE hInstance);
    
    // Run the message loop
    int run();
    
    // Set the screen recorder
    void setScreenRecorder(ScreenRecorder* recorder);
    
private:
    // Window procedure
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Create the UI controls
    void createControls();
    
    // Create the tray icon
    void createTrayIcon(bool isRecording = false);
    
    // Remove the tray icon
    void removeTrayIcon();
    
    // Start recording
    void startRecording();
    
    // Stop recording
    void stopRecording();
    
    // Hide the window
    void hideWindow();
    
    // Show the window
    void showWindow();
    
    // Get values from the UI controls
    std::string getOutputFilename();
    int getFps();
    double getDuration();
    
    // Main window handle
    HWND hwnd;
    
    // UI controls
    HWND outputFilenameEdit;
    HWND fpsEdit;
    HWND durationEdit;
    HWND startButton;
    HWND stopButton;
    
    // Menu items
    HMENU trayMenu;
    
    // Application instance
    HINSTANCE hInstance;
    
    // Tray icon data
    NOTIFYICONDATA trayIconData;
    
    // Recording status
    bool isRecording;
    
    // Screen recorder
    ScreenRecorder* recorder;
}; 