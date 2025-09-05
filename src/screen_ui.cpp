#include "../include/screen_ui.h"
#include "../include/audio_capture.h"
#include "../include/webcam_capture.h"
#include <ctime>
#include <sstream>
#include <iomanip>
#include <shellapi.h>
#include <strsafe.h>

// Resource IDs
#define IDI_TRAYICON 1001
#define IDI_RECORDING 1002
#define IDC_FILENAME 2001
#define IDC_FPS 2002
#define IDC_DURATION 2003
#define IDC_CODEC 2004
#define IDC_QUALITY 2005
#define IDC_START 2006
#define IDC_STOP 2007
#define IDC_ENABLE_AUDIO 2008
#define IDC_MIC_ENABLED 2009
#define IDC_SYSTEM_AUDIO_ENABLED 2010
#define IDC_MIC_DEVICE 2011
#define IDC_SYSTEM_DEVICE 2012
#define IDC_ENABLE_WEBCAM 2013
#define IDC_WEBCAM_DEVICE 2014
#define IDC_OVERLAY_SHAPE 2015
#define IDC_WEBCAM_PREVIEW 2016
#define IDM_EXIT 3001
#define IDM_SHOW 3002

// Window class name
#define WINDOW_CLASS_NAME L"ScreenITWindowClass"

// Helper function to generate default filename based on current time
std::string generateDefaultFilename() {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << "recording_" 
        << std::put_time(&tm, "%Y%m%d_%H%M%S") 
        << ".mp4";
    return oss.str();
}

// For converting std::string to LPCWSTR
std::wstring stringToWideString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// For converting LPCWSTR to std::string
std::string wideStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Constructor
ScreenUI::ScreenUI() : hwnd(NULL), isRecording(false), recorder(nullptr) {
    // Initialize COM for the tray icon
    CoInitialize(NULL);
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icc);
}

// Destructor
ScreenUI::~ScreenUI() {
    // Remove tray icon
    removeTrayIcon();
    
    // Destroy menu
    if (trayMenu) {
        DestroyMenu(trayMenu);
    }
    
    // Uninitialize COM
    CoUninitialize();
}

// Initialize the UI
bool ScreenUI::init(HINSTANCE hInst) {
    this->hInstance = hInst;
    
    // Register the window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    
    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }
    
    // Create the window
    hwnd = CreateWindowEx(
        0,
        WINDOW_CLASS_NAME,
        L"ScreenIT - Screen Recorder",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        NULL, NULL, hInstance, this
    );
    
    if (!hwnd) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }
    
    // Create controls
    createControls();
    
    // Create tray icon
    createTrayIcon();
    
    // Create tray menu
    trayMenu = CreatePopupMenu();
    AppendMenu(trayMenu, MF_STRING, IDM_SHOW, L"Show ScreenIT");
    AppendMenu(trayMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(trayMenu, MF_STRING, IDM_EXIT, L"Exit");
    
    // Show the window
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    return true;
}

// Create the UI controls
void ScreenUI::createControls() {
    // Create labels
    CreateWindowEx(0, L"STATIC", L"Output Filename:", WS_CHILD | WS_VISIBLE,
                   20, 20, 120, 20, hwnd, NULL, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"FPS:", WS_CHILD | WS_VISIBLE,
                   20, 60, 120, 20, hwnd, NULL, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Duration (seconds, 0 = until stopped):", WS_CHILD | WS_VISIBLE,
                   20, 100, 250, 20, hwnd, NULL, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Video Codec:", WS_CHILD | WS_VISIBLE,
                   20, 140, 120, 20, hwnd, NULL, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Quality Preset:", WS_CHILD | WS_VISIBLE,
                   20, 180, 120, 20, hwnd, NULL, hInstance, NULL);
    
    // Create edit controls
    std::string defaultFilename = generateDefaultFilename();
    std::wstring wDefaultFilename = stringToWideString(defaultFilename);
    
    outputFilenameEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", wDefaultFilename.c_str(),
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        160, 20, 200, 20, hwnd, (HMENU)IDC_FILENAME, hInstance, NULL);
    
    fpsEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"30",
                             WS_CHILD | WS_VISIBLE | ES_NUMBER,
                             160, 60, 60, 20, hwnd, (HMENU)IDC_FPS, hInstance, NULL);
    
    durationEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                                  WS_CHILD | WS_VISIBLE | ES_NUMBER,
                                  260, 100, 60, 20, hwnd, (HMENU)IDC_DURATION, hInstance, NULL);
    
    // Create codec combo box
    codecComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                   160, 140, 200, 100, hwnd, (HMENU)IDC_CODEC, hInstance, NULL);
    
    // Populate codec combo box
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"H.264 Hardware (Recommended)");
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"H.264 Software");
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"HEVC Hardware (Smaller files)");
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"HEVC Software");
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"AV1 Hardware (Smallest files)");
    SendMessage(codecComboBox, CB_ADDSTRING, 0, (LPARAM)L"MJPEG (Fallback)");
    SendMessage(codecComboBox, CB_SETCURSEL, 0, 0); // Default to H.264 Hardware
    
    // Create quality combo box
    qualityComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                     WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                     160, 180, 200, 100, hwnd, (HMENU)IDC_QUALITY, hInstance, NULL);
    
    // Populate quality combo box
    SendMessage(qualityComboBox, CB_ADDSTRING, 0, (LPARAM)L"Small & Sharp (Recommended)");
    SendMessage(qualityComboBox, CB_ADDSTRING, 0, (LPARAM)L"Balanced");
    SendMessage(qualityComboBox, CB_ADDSTRING, 0, (LPARAM)L"High Quality");
    SendMessage(qualityComboBox, CB_ADDSTRING, 0, (LPARAM)L"Lossless (Huge files)");
    SendMessage(qualityComboBox, CB_SETCURSEL, 0, 0); // Default to Small & Sharp
    
    // Add ULTRA_TINY to quality options
    SendMessage(qualityComboBox, CB_INSERTSTRING, 0, (LPARAM)L"Ultra Tiny (May pixelate)");
    SendMessage(qualityComboBox, CB_SETCURSEL, 1, 0); // Default to Small & Sharp (now index 1)
    
    // Audio Controls Group
    audioGroupBox = CreateWindowEx(0, L"BUTTON", L"Audio Settings",
                                   WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                   20, 220, 340, 120, hwnd, NULL, hInstance, NULL);
    
    enableAudioCheckbox = CreateWindowEx(0, L"BUTTON", L"Enable Audio Recording",
                                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BST_CHECKED,
                                         30, 240, 160, 20, hwnd, (HMENU)IDC_ENABLE_AUDIO, hInstance, NULL);
    
    microphoneCheckbox = CreateWindowEx(0, L"BUTTON", L"Record Microphone",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BST_CHECKED,
                                       30, 260, 140, 20, hwnd, (HMENU)IDC_MIC_ENABLED, hInstance, NULL);
    
    systemAudioCheckbox = CreateWindowEx(0, L"BUTTON", L"Record System Audio",
                                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BST_CHECKED,
                                         30, 280, 140, 20, hwnd, (HMENU)IDC_SYSTEM_AUDIO_ENABLED, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Mic Device:", WS_CHILD | WS_VISIBLE,
                   180, 260, 70, 20, hwnd, NULL, hInstance, NULL);
    
    micDeviceComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                       WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                       250, 260, 100, 100, hwnd, (HMENU)IDC_MIC_DEVICE, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"System:", WS_CHILD | WS_VISIBLE,
                   180, 280, 50, 20, hwnd, NULL, hInstance, NULL);
    
    systemDeviceComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          230, 280, 120, 100, hwnd, (HMENU)IDC_SYSTEM_DEVICE, hInstance, NULL);
    
    // Webcam Controls Group
    webcamGroupBox = CreateWindowEx(0, L"BUTTON", L"Webcam Settings",
                                    WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                    20, 350, 340, 80, hwnd, NULL, hInstance, NULL);
    
    enableWebcamCheckbox = CreateWindowEx(0, L"BUTTON", L"Enable Webcam Overlay",
                                          WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                          30, 370, 140, 20, hwnd, (HMENU)IDC_ENABLE_WEBCAM, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Device:", WS_CHILD | WS_VISIBLE,
                   180, 370, 50, 20, hwnd, NULL, hInstance, NULL);
    
    webcamDeviceComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          230, 370, 120, 100, hwnd, (HMENU)IDC_WEBCAM_DEVICE, hInstance, NULL);
    
    CreateWindowEx(0, L"STATIC", L"Shape:", WS_CHILD | WS_VISIBLE,
                   30, 395, 50, 20, hwnd, NULL, hInstance, NULL);
    
    overlayShapeComboBox = CreateWindowEx(0, L"COMBOBOX", L"",
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          80, 395, 80, 100, hwnd, (HMENU)IDC_OVERLAY_SHAPE, hInstance, NULL);
    
    SendMessage(overlayShapeComboBox, CB_ADDSTRING, 0, (LPARAM)L"Rectangle");
    SendMessage(overlayShapeComboBox, CB_ADDSTRING, 0, (LPARAM)L"Circle");
    SendMessage(overlayShapeComboBox, CB_SETCURSEL, 0, 0);
    
    webcamPreviewButton = CreateWindowEx(0, L"BUTTON", L"Preview",
                                         WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                         170, 395, 60, 25, hwnd, (HMENU)IDC_WEBCAM_PREVIEW, hInstance, NULL);
    
    // Create buttons (moved down to accommodate new controls)
    startButton = CreateWindowEx(0, L"BUTTON", L"Start Recording",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 80, 450, 120, 30, hwnd, (HMENU)IDC_START, hInstance, NULL);
    
    stopButton = CreateWindowEx(0, L"BUTTON", L"Stop Recording",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                                210, 450, 120, 30, hwnd, (HMENU)IDC_STOP, hInstance, NULL);
    
    // Set fonts for better appearance
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(outputFilenameEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(fpsEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(durationEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(codecComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(qualityComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(startButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(stopButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    
    // Set fonts for new controls
    SendMessage(audioGroupBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(enableAudioCheckbox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(microphoneCheckbox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(systemAudioCheckbox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(micDeviceComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(systemDeviceComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(webcamGroupBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(enableWebcamCheckbox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(webcamDeviceComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(overlayShapeComboBox, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(webcamPreviewButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    
    // Populate device lists
    populateAudioDevices();
    populateWebcamDevices();
}

// Create the tray icon
void ScreenUI::createTrayIcon(bool isRecording) {
    // Initialize tray icon data
    ZeroMemory(&trayIconData, sizeof(NOTIFYICONDATA));
    trayIconData.cbSize = sizeof(NOTIFYICONDATA);
    trayIconData.hWnd = hwnd;
    trayIconData.uID = IDI_TRAYICON;
    trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    trayIconData.uCallbackMessage = WM_TRAYICON;
    
    // Set icon based on recording status
    HICON hIcon = NULL;
    
    // Try to load custom icons
    hIcon = (HICON)LoadImage(
        NULL,
        isRecording ? L"recording.ico" : L"idle.ico",
        IMAGE_ICON,
        0, 0,
        LR_LOADFROMFILE | LR_DEFAULTSIZE
    );
    
    // If custom icon failed to load, use a system icon
    if (!hIcon) {
        hIcon = LoadIcon(NULL, isRecording ? IDI_ERROR : IDI_APPLICATION);
    }
    
    trayIconData.hIcon = hIcon;
    
    // Set tooltip
    StringCchCopy(trayIconData.szTip, 
                  ARRAYSIZE(trayIconData.szTip), 
                  isRecording ? L"ScreenIT (Recording)" : L"ScreenIT");
    
    // Add the icon to the system tray
    Shell_NotifyIcon(NIM_ADD, &trayIconData);
}

// Remove the tray icon
void ScreenUI::removeTrayIcon() {
    if (trayIconData.cbSize > 0) {
        Shell_NotifyIcon(NIM_DELETE, &trayIconData);
        
        if (trayIconData.hIcon) {
            DestroyIcon(trayIconData.hIcon);
        }
    }
}

// Run the message loop
int ScreenUI::run() {
    MSG msg;
    
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

// Set the screen recorder
void ScreenUI::setScreenRecorder(ScreenRecorder* rec) {
    recorder = rec;
}

// Start recording
void ScreenUI::startRecording() {
    if (!recorder || isRecording) {
        return;
    }
    
    std::string outputFilename = getOutputFilename();
    int fps = getFps();
    double duration = getDuration();
    VideoCodec codec = getSelectedCodec();
    QualityPreset quality = getSelectedQuality();
    
    // Get audio and webcam settings
    bool audioEnabled = getAudioEnabled();
    bool micEnabled = getMicrophoneEnabled();
    bool systemAudioEnabled = getSystemAudioEnabled();
    bool webcamEnabled = getWebcamEnabled();
    
    // Start the recording with all settings
    if (recorder->start(outputFilename, fps, duration, codec, quality, 
                       audioEnabled, micEnabled, systemAudioEnabled, webcamEnabled)) {
        isRecording = true;
        
        // Update UI
        EnableWindow(startButton, FALSE);
        EnableWindow(stopButton, TRUE);
        
        // Update tray icon
        removeTrayIcon();
        createTrayIcon(true);
        
        // Hide the window if recording
        hideWindow();
    } else {
        MessageBox(hwnd, L"Failed to start recording!", L"Error", MB_ICONERROR | MB_OK);
    }
}

// Stop recording
void ScreenUI::stopRecording() {
    if (!recorder || !isRecording) {
        return;
    }
    
    // Stop the recording
    recorder->stop();
    isRecording = false;
    
    // Update UI
    EnableWindow(startButton, TRUE);
    EnableWindow(stopButton, FALSE);
    
    // Update tray icon
    removeTrayIcon();
    createTrayIcon(false);
    
    // Show the window again
    showWindow();
    
    // Show success message
    std::string outputFilename = getOutputFilename();
    std::wstring wOutputFilename = stringToWideString(outputFilename);
    
    std::wstring message = L"Recording saved to: " + wOutputFilename;
    MessageBox(hwnd, message.c_str(), L"Recording Completed", MB_ICONINFORMATION | MB_OK);
}

// Hide the window
void ScreenUI::hideWindow() {
    ShowWindow(hwnd, SW_HIDE);
}

// Show the window
void ScreenUI::showWindow() {
    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
}

// Get output filename from the edit control
std::string ScreenUI::getOutputFilename() {
    WCHAR buffer[MAX_PATH];
    GetWindowText(outputFilenameEdit, buffer, MAX_PATH);
    
    std::wstring wstr(buffer);
    return wideStringToString(wstr);
}

// Get FPS from the edit control
int ScreenUI::getFps() {
    WCHAR buffer[16];
    GetWindowText(fpsEdit, buffer, 16);
    
    int fps = _wtoi(buffer);
    return (fps > 0) ? fps : 30; // Default to 30 if invalid
}

// Get duration from the edit control
double ScreenUI::getDuration() {
    WCHAR buffer[16];
    GetWindowText(durationEdit, buffer, 16);
    
    double duration = _wtof(buffer);
    return (duration >= 0) ? duration : 0; // Default to 0 if invalid
}

// Get selected codec from combo box
VideoCodec ScreenUI::getSelectedCodec() {
    int selection = SendMessage(codecComboBox, CB_GETCURSEL, 0, 0);
    
    switch (selection) {
        case 0: return VideoCodec::H264_HARDWARE;
        case 1: return VideoCodec::H264_SOFTWARE;
        case 2: return VideoCodec::HEVC_HARDWARE;
        case 3: return VideoCodec::HEVC_SOFTWARE;
        case 4: return VideoCodec::AV1_HARDWARE;
        case 5: return VideoCodec::MJPEG;
        default: return VideoCodec::H264_HARDWARE; // Default fallback
    }
}

// Get selected quality preset from combo box
QualityPreset ScreenUI::getSelectedQuality() {
    int selection = SendMessage(qualityComboBox, CB_GETCURSEL, 0, 0);
    
    switch (selection) {
        case 0: return QualityPreset::SMALL_SHARP;
        case 1: return QualityPreset::BALANCED;
        case 2: return QualityPreset::HIGH_QUALITY;
        case 3: return QualityPreset::LOSSLESS;
        default: return QualityPreset::SMALL_SHARP; // Default fallback
    }
}

// Window procedure
LRESULT CALLBACK ScreenUI::windowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ScreenUI* pThis = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (ScreenUI*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (ScreenUI*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }
    
    if (pThis) {
        switch (uMsg) {
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDC_START:
                        pThis->startRecording();
                        return 0;
                        
                    case IDC_STOP:
                        pThis->stopRecording();
                        return 0;
                        
                    case IDM_EXIT:
                        if (pThis->isRecording) {
                            pThis->stopRecording();
                        }
                        DestroyWindow(hwnd);
                        return 0;
                        
                    case IDM_SHOW:
                        pThis->showWindow();
                        return 0;
                }
                break;
                
            case WM_TRAYICON:
                if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    SetForegroundWindow(hwnd);
                    
                    // Show context menu
                    TrackPopupMenu(pThis->trayMenu, TPM_RIGHTBUTTON, 
                                   pt.x, pt.y, 0, hwnd, NULL);
                    PostMessage(hwnd, WM_NULL, 0, 0);
                } else if (lParam == WM_LBUTTONDBLCLK) {
                    // Show the window on double-click
                    pThis->showWindow();
                }
                return 0;
                
            case WM_CLOSE:
                if (pThis->isRecording) {
                    if (MessageBox(hwnd, L"Recording is in progress. Stop recording and exit?", 
                                  L"ScreenIT", MB_ICONQUESTION | MB_YESNO) == IDYES) {
                        pThis->stopRecording();
                        DestroyWindow(hwnd);
                    }
                } else {
                    DestroyWindow(hwnd);
                }
                return 0;
                
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Audio/Webcam control methods
bool ScreenUI::getAudioEnabled() {
    return SendMessage(enableAudioCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool ScreenUI::getMicrophoneEnabled() {
    return SendMessage(microphoneCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool ScreenUI::getSystemAudioEnabled() {
    return SendMessage(systemAudioCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

bool ScreenUI::getWebcamEnabled() {
    return SendMessage(enableWebcamCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void ScreenUI::populateAudioDevices() {
    if (!recorder) return;
    
    // Clear existing items
    SendMessage(micDeviceComboBox, CB_RESETCONTENT, 0, 0);
    SendMessage(systemDeviceComboBox, CB_RESETCONTENT, 0, 0);
    
    // Get audio devices
    auto devices = recorder->getAudioDevices();
    
    for (const auto& device : devices) {
        std::wstring wName = stringToWideString(device.name);
        
        if (device.isInput) {
            // Microphone device
            int index = SendMessage(micDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
            if (device.isDefault) {
                SendMessage(micDeviceComboBox, CB_SETCURSEL, index, 0);
            }
        } else {
            // System audio device
            int index = SendMessage(systemDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
            if (device.isDefault) {
                SendMessage(systemDeviceComboBox, CB_SETCURSEL, index, 0);
            }
        }
    }
}

void ScreenUI::populateWebcamDevices() {
    if (!recorder) return;
    
    // Clear existing items
    SendMessage(webcamDeviceComboBox, CB_RESETCONTENT, 0, 0);
    
    // Get webcam devices
    auto devices = recorder->getWebcamDevices();
    
    for (const auto& device : devices) {
        std::wstring wName = stringToWideString(device.name);
        int index = SendMessage(webcamDeviceComboBox, CB_ADDSTRING, 0, (LPARAM)wName.c_str());
        if (device.isDefault) {
            SendMessage(webcamDeviceComboBox, CB_SETCURSEL, index, 0);
        }
    }
}

void ScreenUI::onAudioDeviceChanged() {
    // Handle audio device selection change
    // Implementation can be added later for real-time device switching
}

void ScreenUI::onWebcamDeviceChanged() {
    // Handle webcam device selection change
    // Implementation can be added later for real-time device switching
} 