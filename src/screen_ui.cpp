#include "../include/screen_ui.h"
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
#define IDC_START 2004
#define IDC_STOP 2005
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
        << ".avi";
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
    
    // Create buttons
    startButton = CreateWindowEx(0, L"BUTTON", L"Start Recording",
                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                 80, 160, 120, 30, hwnd, (HMENU)IDC_START, hInstance, NULL);
    
    stopButton = CreateWindowEx(0, L"BUTTON", L"Stop Recording",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
                                200, 160, 120, 30, hwnd, (HMENU)IDC_STOP, hInstance, NULL);
    
    // Set fonts for better appearance
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    SendMessage(outputFilenameEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(fpsEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(durationEdit, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(startButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
    SendMessage(stopButton, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));
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
    
    // Start the recording
    if (recorder->start(outputFilename, fps, duration)) {
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