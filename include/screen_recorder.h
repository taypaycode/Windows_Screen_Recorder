#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>

#include "video_types.h"

#ifdef HAVE_FFMPEG
// Forward declaration to avoid circular includes
class FFmpegEncoder;
#endif

// Forward declaration for audio capture
class AudioCapture;

// Forward declaration for webcam capture  
class WebcamCapture;

class ScreenRecorder {
public:
    ScreenRecorder();
    ~ScreenRecorder();
    
    // Start recording with given parameters
    bool start(const std::string& outputFilename, int fps = 30, double durationSec = 0, 
               VideoCodec codec = VideoCodec::H264_HARDWARE, 
               QualityPreset quality = QualityPreset::SMALL_SHARP,
               bool enableAudio = true, bool enableMicrophone = true, bool enableSystemAudio = true,
               bool enableWebcam = false);
    
    // Stop recording
    void stop();
    
    // Check if recording is in progress
    bool isRecording() const;
    
    // Get available hardware encoders
    static std::vector<VideoCodec> getAvailableCodecs();
    
    // Get estimated file size for given duration and settings
    static double estimateFileSize(int width, int height, int fps, double durationSec, 
                                   VideoCodec codec, QualityPreset quality);

    // Audio device management
    std::vector<struct AudioDevice> getAudioDevices();
    bool setMicrophoneDevice(const std::string& deviceId);
    bool setSystemAudioDevice(const std::string& deviceId);
    
    // Audio control
    bool isAudioEnabled() const { return audioEnabled; }
    void setAudioEnabled(bool enabled) { audioEnabled = enabled; }
    float getMicrophoneLevel() const;
    float getSystemAudioLevel() const;
    
    // Webcam device management
    std::vector<struct WebcamDevice> getWebcamDevices();
    bool setWebcamDevice(int deviceId);
    
    // Webcam control
    bool isWebcamEnabled() const { return webcamEnabled; }
    void setWebcamEnabled(bool enabled) { webcamEnabled = enabled; }
    void setWebcamOverlayEnabled(bool enabled);
    void setWebcamOverlayProperties(float x, float y, float width, float height, int shape);
    bool isWebcamOverlayEnabled() const;

private:
    // Recording thread function
    void recordingThread();
    
    // Audio processing thread function
    void audioProcessingThread();
    
    // Capture screen frame
    bool captureScreen(cv::Mat& frame);
    
    // Initialize video writer with codec and quality settings
    bool initializeVideoWriter(const std::string& filename, VideoCodec codec, QualityPreset quality);
    
    // Get OpenCV fourcc code for given codec
    int getCodecFourCC(VideoCodec codec);
    
    // Get bitrate for quality preset
    int getBitrate(VideoCodec codec, QualityPreset quality);
    
    // Video writer for output
    cv::VideoWriter videoWriter;
    
#ifdef HAVE_FFMPEG
    // FFmpeg encoder (preferred)
    std::unique_ptr<FFmpegEncoder> ffmpegEncoder;
    bool useFFmpeg;
#endif
    
    // Recording control
    std::atomic<bool> recording;
    std::string outputFile;
    int frameRate;
    double duration;
    std::thread recThread;
    std::thread audioThread;
    
    // Audio capture
    std::unique_ptr<AudioCapture> audioCapture;
    std::atomic<bool> audioEnabled;
    std::atomic<bool> microphoneEnabled;
    std::atomic<bool> systemAudioEnabled;
    
    // Webcam capture
    std::unique_ptr<WebcamCapture> webcamCapture;
    std::atomic<bool> webcamEnabled;
    
    // Encoding settings
    VideoCodec currentCodec;
    QualityPreset currentQuality;
    
    // Screen dimensions
    int screenWidth;
    int screenHeight;
    
    // Screen position (for multi-monitor setups)
    int screenLeft;
    int screenTop;
}; 