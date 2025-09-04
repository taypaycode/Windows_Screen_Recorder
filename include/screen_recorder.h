#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>

#include "video_types.h"

#ifdef HAVE_FFMPEG
// Forward declaration to avoid circular includes
class FFmpegEncoder;
#endif

class ScreenRecorder {
public:
    ScreenRecorder();
    ~ScreenRecorder();
    
    // Start recording with given parameters
    bool start(const std::string& outputFilename, int fps = 30, double durationSec = 0, 
               VideoCodec codec = VideoCodec::H264_HARDWARE, 
               QualityPreset quality = QualityPreset::SMALL_SHARP);
    
    // Stop recording
    void stop();
    
    // Check if recording is in progress
    bool isRecording() const;
    
    // Get available hardware encoders
    static std::vector<VideoCodec> getAvailableCodecs();
    
    // Get estimated file size for given duration and settings
    static double estimateFileSize(int width, int height, int fps, double durationSec, 
                                   VideoCodec codec, QualityPreset quality);

private:
    // Recording thread function
    void recordingThread();
    
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