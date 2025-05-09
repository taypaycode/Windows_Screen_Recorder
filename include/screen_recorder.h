#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

class ScreenRecorder {
public:
    ScreenRecorder();
    ~ScreenRecorder();
    
    // Start recording with given parameters
    bool start(const std::string& outputFilename, int fps = 30, double durationSec = 0);
    
    // Stop recording
    void stop();
    
    // Check if recording is in progress
    bool isRecording() const;

private:
    // Recording thread function
    void recordingThread();
    
    // Capture screen frame
    bool captureScreen(cv::Mat& frame);
    
    // Video writer for output
    cv::VideoWriter videoWriter;
    
    // Recording control
    std::atomic<bool> recording;
    std::string outputFile;
    int frameRate;
    double duration;
    std::thread recThread;
    
    // Screen dimensions
    int screenWidth;
    int screenHeight;
    
    // Screen position (for multi-monitor setups)
    int screenLeft;
    int screenTop;
}; 