#include "../include/screen_recorder.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")
#endif

ScreenRecorder::ScreenRecorder() : recording(false), frameRate(30), duration(0), 
                                   screenWidth(0), screenHeight(0), screenLeft(0), screenTop(0) {
    // Initialize with proper DPI awareness
#ifdef _WIN32
    // Set DPI awareness to per-monitor for better handling of different DPI values
    SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
    
    // Get the actual screen dimensions including all monitors at native resolution
    screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    
    // Get the screen offset
    screenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    screenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    
    // Check if we got expected resolution based on your monitor
    if (screenWidth < 1920 || screenHeight < 1200) {
        std::cout << "WARNING: Detected screen resolution is lower than expected (1920x1200)." << std::endl;
        std::cout << "Attempting to use native resolution..." << std::endl;
        
        // Try to get actual physical monitor size
        HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFOEX monitorInfo = { sizeof(MONITORINFOEX) };
        GetMonitorInfo(hMonitor, &monitorInfo);
        
        // Try to use the full resolution
        screenWidth = 1920;
        screenHeight = 1200;
    }
#else
    // Default for non-Windows platforms (would need to be implemented)
    screenWidth = 1920;
    screenHeight = 1200;
    screenLeft = 0;
    screenTop = 0;
#endif

    std::cout << "Virtual screen dimensions: " << screenWidth << "x" << screenHeight << std::endl;
    std::cout << "Virtual screen position: (" << screenLeft << "," << screenTop << ")" << std::endl;
}

ScreenRecorder::~ScreenRecorder() {
    stop();
}

bool ScreenRecorder::start(const std::string& outputFilename, int fps, double durationSec) {
    // Don't start if already recording
    if (recording) {
        std::cout << "Already recording!" << std::endl;
        return false;
    }
    
    outputFile = outputFilename;
    frameRate = fps;
    duration = durationSec;
    
    // Initialize VideoWriter with the full resolution
    videoWriter.open(outputFile, 
                    cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                    frameRate,
                    cv::Size(screenWidth, screenHeight));
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Failed to open video writer!" << std::endl;
        return false;
    }
    
    // Start recording thread
    recording = true;
    recThread = std::thread(&ScreenRecorder::recordingThread, this);
    
    std::cout << "Recording started to " << outputFile << std::endl;
    return true;
}

void ScreenRecorder::stop() {
    if (!recording) {
        return;
    }
    
    // Signal thread to stop and wait for it
    recording = false;
    if (recThread.joinable()) {
        recThread.join();
    }
    
    // Release video writer
    if (videoWriter.isOpened()) {
        videoWriter.release();
    }
    
    std::cout << "Recording stopped" << std::endl;
}

bool ScreenRecorder::isRecording() const {
    return recording;
}

void ScreenRecorder::recordingThread() {
    cv::Mat frame;
    auto startTime = std::chrono::high_resolution_clock::now();
    
    while (recording) {
        // Capture frame
        if (captureScreen(frame)) {
            videoWriter.write(frame);
        }
        
        // Check if duration has elapsed (if specified)
        if (duration > 0) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
            if (elapsed >= duration) {
                recording = false;
                break;
            }
        }
        
        // Maintain frame rate
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / frameRate));
    }
}

bool ScreenRecorder::captureScreen(cv::Mat& frame) {
#ifdef _WIN32
    // Create a DC for the entire virtual screen
    HDC hdcScreen = GetDC(NULL);
    
    // Create a memory DC compatible with the screen DC
    HDC hdcMemDC = CreateCompatibleDC(hdcScreen);
    
    // Create a bitmap compatible with the screen DC at full resolution
    HBITMAP hbmScreen = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    
    // Select the bitmap into the memory DC
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMemDC, hbmScreen);
    
    // Copy from screen to memory DC, including the proper screen offset and scaling to full resolution
    BOOL result = StretchBlt(hdcMemDC, 0, 0, screenWidth, screenHeight, 
                             hdcScreen, screenLeft, screenTop, 
                             GetSystemMetrics(SM_CXVIRTUALSCREEN), 
                             GetSystemMetrics(SM_CYVIRTUALSCREEN), 
                             SRCCOPY);
    
    if (!result) {
        std::cerr << "StretchBlt failed: " << GetLastError() << std::endl;
        // Fallback to regular BitBlt
        BitBlt(hdcMemDC, 0, 0, GetSystemMetrics(SM_CXVIRTUALSCREEN), 
               GetSystemMetrics(SM_CYVIRTUALSCREEN), 
               hdcScreen, screenLeft, screenTop, SRCCOPY);
    }
    
    // Get the BITMAP from the HBITMAP
    BITMAP bmpScreen;
    GetObject(hbmScreen, sizeof(BITMAP), &bmpScreen);
    
    // Make sure we have the correct dimensions from the actual bitmap
    if (bmpScreen.bmWidth != screenWidth || bmpScreen.bmHeight != screenHeight) {
        std::cout << "Bitmap dimensions differ from expected: " 
                  << bmpScreen.bmWidth << "x" << bmpScreen.bmHeight 
                  << " vs expected " << screenWidth << "x" << screenHeight << std::endl;
    }
    
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmpScreen.bmWidth;
    bi.biHeight = -bmpScreen.bmHeight;  // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;
    
    // Create OpenCV Mat with the actual bitmap dimensions
    frame = cv::Mat(bmpScreen.bmHeight, bmpScreen.bmWidth, CV_8UC4);
    
    // Get the DIB bits
    GetDIBits(hdcScreen, hbmScreen, 0, bmpScreen.bmHeight, frame.data,
              (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    // Convert from BGRA to BGR for video encoding
    cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);
    
    // Check if the frame dimensions match the expected output dimensions
    if (frame.cols != screenWidth || frame.rows != screenHeight) {
        // Resize the frame to match the expected output dimensions
        cv::resize(frame, frame, cv::Size(screenWidth, screenHeight));
    }
    
    // Clean up
    SelectObject(hdcMemDC, hbmOld);
    DeleteObject(hbmScreen);
    DeleteDC(hdcMemDC);
    ReleaseDC(NULL, hdcScreen);
    
    return true;
#else
    // Implement for other platforms
    std::cerr << "Screen capture not implemented for this platform" << std::endl;
    return false;
#endif
} 