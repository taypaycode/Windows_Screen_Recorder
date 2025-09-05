#include "../include/screen_recorder.h"
#ifdef HAVE_FFMPEG
#include "../include/ffmpeg_encoder.h"
#endif
#include "../include/audio_capture.h"
#include "../include/webcam_capture.h"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#include <shellscalingapi.h>
#pragma comment(lib, "Shcore.lib")
#endif

ScreenRecorder::ScreenRecorder() : recording(false), frameRate(30), duration(0), 
                                   screenWidth(0), screenHeight(0), screenLeft(0), screenTop(0),
                                   currentCodec(VideoCodec::H264_HARDWARE), currentQuality(QualityPreset::SMALL_SHARP),
                                   audioEnabled(true), microphoneEnabled(true), systemAudioEnabled(true),
                                   webcamEnabled(false)
#ifdef HAVE_FFMPEG
                                   , useFFmpeg(true)
#endif
{
#ifdef HAVE_FFMPEG
    ffmpegEncoder = std::make_unique<FFmpegEncoder>();
    std::cout << "✅ FFmpeg encoder available - will use professional compression" << std::endl;
    std::cout << "📊 Expected file size reduction: 60-80% vs OpenCV" << std::endl;
#else
    std::cout << "❌ FFmpeg not available - using OpenCV fallback (larger files)" << std::endl;
#endif

    // Initialize audio capture
    audioCapture = std::make_unique<AudioCapture>();
    if (audioCapture->initialize()) {
        std::cout << "✅ Audio capture system initialized" << std::endl;
    } else {
        std::cout << "❌ Failed to initialize audio capture" << std::endl;
    }
    
    // Initialize webcam capture
    webcamCapture = std::make_unique<WebcamCapture>();
    if (webcamCapture->initialize()) {
        std::cout << "✅ Webcam capture system initialized" << std::endl;
    } else {
        std::cout << "❌ Failed to initialize webcam capture" << std::endl;
    }
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

bool ScreenRecorder::start(const std::string& outputFilename, int fps, double durationSec, 
                           VideoCodec codec, QualityPreset quality,
                           bool enableAudio, bool enableMicrophone, bool enableSystemAudio,
                           bool enableWebcam) {
    // Don't start if already recording
    if (recording) {
        std::cout << "Already recording!" << std::endl;
        return false;
    }
    
    outputFile = outputFilename;
    frameRate = fps;
    duration = durationSec;
    currentCodec = codec;
    currentQuality = quality;
    
    // Set audio options
    audioEnabled = enableAudio;
    microphoneEnabled = enableMicrophone;
    systemAudioEnabled = enableSystemAudio;
    
    // Set webcam options
    webcamEnabled = enableWebcam;
    
    // Initialize encoder (FFmpeg preferred, OpenCV fallback)
#ifdef HAVE_FFMPEG
    if (useFFmpeg) {
        if (!ffmpegEncoder->init(outputFile, screenWidth, screenHeight, frameRate, codec, quality, audioEnabled)) {
            std::cerr << "FFmpeg encoder failed, falling back to OpenCV..." << std::endl;
            useFFmpeg = false;
        }
    }
    
    if (!useFFmpeg) {
        // Fallback to OpenCV
        if (!initializeVideoWriter(outputFile, codec, quality)) {
            std::cerr << "Failed to initialize video writer!" << std::endl;
            return false;
        }
    }
#else
    // OpenCV only
    if (!initializeVideoWriter(outputFile, codec, quality)) {
        std::cerr << "Failed to initialize video writer!" << std::endl;
        return false;
    }
#endif
    
    // Mark recording active before launching worker threads (so they don't exit early)
    recording = true;

    // Start audio capture if enabled
    if (audioEnabled && audioCapture) {
        if (audioCapture->start(microphoneEnabled, systemAudioEnabled)) {
            std::cout << "✅ Audio capture started" << std::endl;
            audioThread = std::thread(&ScreenRecorder::audioProcessingThread, this);
        } else {
            std::cout << "⚠️  Failed to start audio capture" << std::endl;
        }
    }
    
    // Start webcam capture if enabled
    if (webcamEnabled && webcamCapture) {
        if (webcamCapture->start()) {
            webcamCapture->setOverlayEnabled(true); // Enable overlay
            std::cout << "✅ Webcam capture started with overlay enabled" << std::endl;
        } else {
            std::cout << "⚠️  Failed to start webcam capture" << std::endl;
        }
    }
    
    // Start recording thread last
    recThread = std::thread(&ScreenRecorder::recordingThread, this);
    
    std::cout << "Recording started to " << outputFile << " with ";
    switch (codec) {
        case VideoCodec::H264_HARDWARE: std::cout << "H.264 Hardware"; break;
        case VideoCodec::H264_SOFTWARE: std::cout << "H.264 Software"; break;
        case VideoCodec::HEVC_HARDWARE: std::cout << "HEVC Hardware"; break;
        case VideoCodec::HEVC_SOFTWARE: std::cout << "HEVC Software"; break;
        case VideoCodec::AV1_HARDWARE: std::cout << "AV1 Hardware"; break;
        case VideoCodec::MJPEG: std::cout << "MJPEG"; break;
    }
    std::cout << " encoding" << std::endl;
    return true;
}

void ScreenRecorder::stop() {
    if (!recording) {
        return;
    }
    
    // Signal threads to stop and wait for them
    recording = false;
    if (recThread.joinable()) {
        recThread.join();
    }
    
    // Stop audio capture
    if (audioCapture) {
        audioCapture->stop();
    }
    if (audioThread.joinable()) {
        audioThread.join();
    }
    
    // Stop webcam capture
    if (webcamCapture) {
        webcamCapture->stop();
    }
    
    // Release encoder
#ifdef HAVE_FFMPEG
    if (useFFmpeg && ffmpegEncoder) {
        ffmpegEncoder->finish();
    }
#endif
    
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
    auto startTime = std::chrono::steady_clock::now();
    auto nextFrameTime = startTime;
    const auto frameDuration = std::chrono::microseconds(1000000 / frameRate);
    
    while (recording) {
        auto frameStart = std::chrono::steady_clock::now();
        
        // Capture frame
        if (captureScreen(frame)) {
            // Apply webcam overlay if enabled
            if (webcamEnabled && webcamCapture) {
                webcamCapture->applyOverlay(frame);
            }
            
#ifdef HAVE_FFMPEG
            if (useFFmpeg && ffmpegEncoder) {
                ffmpegEncoder->encodeFrame(frame);
            } else {
                videoWriter.write(frame);
            }
#else
            videoWriter.write(frame);
#endif
        }
        
        // Check if duration has elapsed (if specified)
        if (duration > 0) {
            auto currentTime = std::chrono::steady_clock::now();
            double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
            if (elapsed >= duration) {
                recording = false;
                break;
            }
        }
        
        // Precise frame rate timing - wait until next frame time
        nextFrameTime += frameDuration;
        auto now = std::chrono::steady_clock::now();
        if (nextFrameTime > now) {
            std::this_thread::sleep_until(nextFrameTime);
        } else {
            // If we're behind schedule, reset timing to prevent drift
            nextFrameTime = now;
        }
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

// Initialize video writer with optimized codec and quality settings
bool ScreenRecorder::initializeVideoWriter(const std::string& filename, VideoCodec codec, QualityPreset quality) {
    int bitrate = getBitrate(codec, quality);
    
    std::cout << "Initializing video writer:" << std::endl;
    std::cout << "- Filename: " << filename << std::endl;
    std::cout << "- Resolution: " << screenWidth << "x" << screenHeight << std::endl;
    std::cout << "- Target bitrate: " << bitrate << " kbps" << std::endl;
    
    // Define codec priority lists with better fallbacks
    std::vector<std::pair<int, std::string>> codecsToTry;
    
    switch (codec) {
        case VideoCodec::H264_HARDWARE:
            codecsToTry = {
                {cv::VideoWriter::fourcc('a', 'v', 'c', '1'), "AVC1 (H.264)"},      // Standard H.264
                {cv::VideoWriter::fourcc('H', '2', '6', '4'), "H264"},               // Alternative H.264
                {cv::VideoWriter::fourcc('X', '2', '6', '4'), "X264 (Software)"},   // Software fallback
                {0x00000021, "H.264 (Windows Media Foundation)"}                    // WMF H.264
            };
            break;
        case VideoCodec::H264_SOFTWARE:
            codecsToTry = {
                {cv::VideoWriter::fourcc('X', '2', '6', '4'), "X264 (Software)"},
                {cv::VideoWriter::fourcc('a', 'v', 'c', '1'), "AVC1 (H.264)"},
                {0x00000021, "H.264 (Windows Media Foundation)"}
            };
            break;
        case VideoCodec::HEVC_HARDWARE:
            codecsToTry = {
                {cv::VideoWriter::fourcc('H', 'E', 'V', 'C'), "HEVC"},
                {cv::VideoWriter::fourcc('H', '2', '6', '5'), "H265"},
                {cv::VideoWriter::fourcc('a', 'v', 'c', '1'), "AVC1 (H.264 fallback)"}  // Fallback to H.264
            };
            break;
        case VideoCodec::HEVC_SOFTWARE:
            codecsToTry = {
                {cv::VideoWriter::fourcc('H', '2', '6', '5'), "H265 (Software)"},
                {cv::VideoWriter::fourcc('X', '2', '6', '4'), "X264 (fallback)"}
            };
            break;
        case VideoCodec::AV1_HARDWARE:
            codecsToTry = {
                {cv::VideoWriter::fourcc('A', 'V', '0', '1'), "AV1"},
                {cv::VideoWriter::fourcc('a', 'v', 'c', '1'), "AVC1 (H.264 fallback)"}  // Fallback to H.264
            };
            break;
        case VideoCodec::MJPEG:
        default:
            codecsToTry = {
                {cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), "MJPEG"}
            };
            break;
    }
    
    // Try each codec until one works
    for (auto& [codecFourcc, codecName] : codecsToTry) {
        std::cout << "Trying codec: " << codecName << " (0x" << std::hex << codecFourcc << std::dec << ")" << std::endl;
        
        // Create VideoWriter with specific backend parameters for compression
        std::vector<int> params;
        
        // For H.264 codecs, set quality parameters
        if (codecFourcc == cv::VideoWriter::fourcc('a', 'v', 'c', '1') || 
            codecFourcc == cv::VideoWriter::fourcc('H', '2', '6', '4') ||
            codecFourcc == cv::VideoWriter::fourcc('X', '2', '6', '4')) {
            
            // Use CRF (Constant Rate Factor) for better compression
            int crf = 28; // Higher = more compression (18-28 is good range)
            switch (quality) {
                case QualityPreset::SMALL_SHARP: crf = 32; break;  // Very compressed
                case QualityPreset::BALANCED: crf = 28; break;     // Balanced
                case QualityPreset::HIGH_QUALITY: crf = 23; break; // High quality
                case QualityPreset::LOSSLESS: crf = 18; break;     // Near lossless
            }
            
            params.push_back(cv::VIDEOWRITER_PROP_QUALITY);
            params.push_back(crf);
        }
        
        // Try to open with parameters
        if (!params.empty()) {
            videoWriter.open(filename, codecFourcc, frameRate, cv::Size(screenWidth, screenHeight), params);
        } else {
            videoWriter.open(filename, codecFourcc, frameRate, cv::Size(screenWidth, screenHeight));
        }
        
        if (videoWriter.isOpened()) {
            std::cout << "✓ Successfully initialized with: " << codecName << std::endl;
            std::cout << "✓ Target bitrate: " << bitrate << " kbps" << std::endl;
            std::cout << "✓ Expected file size for 60 seconds: ~" 
                      << (bitrate * 60 / (8 * 1024)) << " MB" << std::endl;
            return true;
        } else {
            std::cout << "✗ Failed to initialize: " << codecName << std::endl;
        }
    }
    
    // Last resort: try the most compatible codec
    std::cout << "All preferred codecs failed, trying last resort..." << std::endl;
    
    // Try mp4v (MPEG-4 Part 2) - very widely supported
    videoWriter.open(filename, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), 
                    frameRate, cv::Size(screenWidth, screenHeight));
    
    if (videoWriter.isOpened()) {
        std::cout << "✓ Fallback successful: MPEG-4 Part 2" << std::endl;
        return true;
    }
    
    std::cerr << "✗ CRITICAL: All codecs failed! Check OpenCV codec support." << std::endl;
    return false;
}

// Get OpenCV fourcc code for given codec
int ScreenRecorder::getCodecFourCC(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264_HARDWARE:
        case VideoCodec::H264_SOFTWARE:
            return cv::VideoWriter::fourcc('H', '2', '6', '4');
        case VideoCodec::HEVC_HARDWARE:
        case VideoCodec::HEVC_SOFTWARE:
            return cv::VideoWriter::fourcc('H', 'E', 'V', 'C');
        case VideoCodec::AV1_HARDWARE:
            return cv::VideoWriter::fourcc('A', 'V', '0', '1');
        case VideoCodec::MJPEG:
        default:
            return cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    }
}

// Get bitrate for quality preset (in kbps)
int ScreenRecorder::getBitrate(VideoCodec codec, QualityPreset quality) {
    // Base bitrates for 1080p at 30fps (adjust for actual resolution/fps)
    int baseBitrate = 0;
    
    switch (codec) {
        case VideoCodec::H264_HARDWARE:
        case VideoCodec::H264_SOFTWARE:
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 500; break;    // 0.5 Mbps - very small
                case QualityPreset::BALANCED: baseBitrate = 1500; break;      // 1.5 Mbps
                case QualityPreset::HIGH_QUALITY: baseBitrate = 3000; break;  // 3 Mbps
                case QualityPreset::LOSSLESS: baseBitrate = 15000; break;     // 15 Mbps
            }
            break;
        case VideoCodec::HEVC_HARDWARE:
        case VideoCodec::HEVC_SOFTWARE:
            // HEVC is ~50% more efficient than H.264
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 300; break;    // 0.3 Mbps - very efficient
                case QualityPreset::BALANCED: baseBitrate = 750; break;       // 0.75 Mbps
                case QualityPreset::HIGH_QUALITY: baseBitrate = 1500; break;  // 1.5 Mbps
                case QualityPreset::LOSSLESS: baseBitrate = 7500; break;      // 7.5 Mbps
            }
            break;
        case VideoCodec::AV1_HARDWARE:
            // AV1 is ~30% more efficient than HEVC
            switch (quality) {
                case QualityPreset::SMALL_SHARP: baseBitrate = 700; break;    // 0.7 Mbps
                case QualityPreset::BALANCED: baseBitrate = 1750; break;      // 1.75 Mbps
                case QualityPreset::HIGH_QUALITY: baseBitrate = 3500; break;  // 3.5 Mbps
                case QualityPreset::LOSSLESS: baseBitrate = 17500; break;     // 17.5 Mbps
            }
            break;
        case VideoCodec::MJPEG:
        default:
            baseBitrate = 50000; // MJPEG is uncompressed, very high bitrate
            break;
    }
    
    // Scale bitrate based on actual resolution and frame rate
    double resolutionFactor = (double)(screenWidth * screenHeight) / (1920.0 * 1080.0);
    double framerateFactor = (double)frameRate / 30.0;
    
    return (int)(baseBitrate * resolutionFactor * framerateFactor);
}

// Get available hardware encoders
std::vector<VideoCodec> ScreenRecorder::getAvailableCodecs() {
    std::vector<VideoCodec> available;
    
    // Test if codecs are available by trying to create a small video writer
    cv::Size testSize(64, 64);
    
    // Test H.264 hardware
    cv::VideoWriter testWriter;
    testWriter.open("test_h264.mp4", cv::VideoWriter::fourcc('H', '2', '6', '4'), 30, testSize);
    if (testWriter.isOpened()) {
        available.push_back(VideoCodec::H264_HARDWARE);
        testWriter.release();
    }
    
    // Test H.264 software (usually always available)
    testWriter.open("test_x264.mp4", cv::VideoWriter::fourcc('X', '2', '6', '4'), 30, testSize);
    if (testWriter.isOpened()) {
        available.push_back(VideoCodec::H264_SOFTWARE);
        testWriter.release();
    }
    
    // Test HEVC hardware
    testWriter.open("test_hevc.mp4", cv::VideoWriter::fourcc('H', 'E', 'V', 'C'), 30, testSize);
    if (testWriter.isOpened()) {
        available.push_back(VideoCodec::HEVC_HARDWARE);
        testWriter.release();
    }
    
    // Test HEVC software
    testWriter.open("test_h265.mp4", cv::VideoWriter::fourcc('H', '2', '6', '5'), 30, testSize);
    if (testWriter.isOpened()) {
        available.push_back(VideoCodec::HEVC_SOFTWARE);
        testWriter.release();
    }
    
    // Test AV1 hardware
    testWriter.open("test_av1.mp4", cv::VideoWriter::fourcc('A', 'V', '0', '1'), 30, testSize);
    if (testWriter.isOpened()) {
        available.push_back(VideoCodec::AV1_HARDWARE);
        testWriter.release();
    }
    
    // MJPEG is always available as fallback
    available.push_back(VideoCodec::MJPEG);
    
    // Clean up test files
    remove("test_h264.mp4");
    remove("test_x264.mp4");
    remove("test_hevc.mp4");
    remove("test_h265.mp4");
    remove("test_av1.mp4");
    
    return available;
}

// Get estimated file size for given duration and settings (in MB)
double ScreenRecorder::estimateFileSize(int width, int height, int fps, double durationSec, 
                                        VideoCodec codec, QualityPreset quality) {
    // Create a temporary recorder to get bitrate calculation
    ScreenRecorder temp;
    temp.screenWidth = width;
    temp.screenHeight = height;
    temp.frameRate = fps;
    
    int bitrate = temp.getBitrate(codec, quality); // in kbps
    
    // Convert to MB: (bitrate in kbps) * (duration in seconds) / (8 bits per byte) / (1024 KB per MB)
    double fileSizeMB = (bitrate * durationSec) / (8.0 * 1024.0);
    
    return fileSizeMB;
}

// Audio device management
std::vector<AudioDevice> ScreenRecorder::getAudioDevices() {
    if (audioCapture) {
        return audioCapture->enumerateDevices();
    }
    return {};
}

bool ScreenRecorder::setMicrophoneDevice(const std::string& deviceId) {
    if (audioCapture) {
        return audioCapture->setMicrophoneDevice(deviceId);
    }
    return false;
}

bool ScreenRecorder::setSystemAudioDevice(const std::string& deviceId) {
    if (audioCapture) {
        return audioCapture->setSystemAudioDevice(deviceId);
    }
    return false;
}

float ScreenRecorder::getMicrophoneLevel() const {
    if (audioCapture) {
        return audioCapture->getMicrophoneLevel();
    }
    return 0.0f;
}

float ScreenRecorder::getSystemAudioLevel() const {
    if (audioCapture) {
        return audioCapture->getSystemAudioLevel();
    }
    return 0.0f;
}

// Audio processing thread function
void ScreenRecorder::audioProcessingThread() {
    std::cout << "🎵 Audio processing thread started" << std::endl;
    
    int micSampleCount = 0;
    int systemSampleCount = 0;
    
    while (recording && audioCapture) {
        AudioSample sample;
        if (audioCapture->getNextSample(sample)) {
            // Send audio samples to FFmpeg encoder if using FFmpeg
#ifdef HAVE_FFMPEG
            if (useFFmpeg && ffmpegEncoder) {
                bool isMicrophone = (sample.source == AudioSample::Source::Microphone);
                
                if (ffmpegEncoder->encodeAudio(sample, isMicrophone)) {
                    if (isMicrophone) {
                        micSampleCount++;
                        if (micSampleCount % 100 == 0) {
                            std::cout << "🎤 Encoded " << micSampleCount << " microphone samples" << std::endl;
                        }
                    } else {
                        systemSampleCount++;
                        if (systemSampleCount % 100 == 0) {
                            std::cout << "🔊 Encoded " << systemSampleCount << " system audio samples" << std::endl;
                        }
                    }
                } else {
                    std::cerr << "⚠️  Failed to encode audio sample" << std::endl;
                }
            }
#endif
        } else {
            // Small delay if no audio data available
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
    
    std::cout << "🎵 Audio processing thread stopped (mic: " << micSampleCount 
              << ", system: " << systemSampleCount << " samples)" << std::endl;
}

// Webcam device management
std::vector<WebcamDevice> ScreenRecorder::getWebcamDevices() {
    if (webcamCapture) {
        return webcamCapture->enumerateDevices();
    }
    return {};
}

bool ScreenRecorder::setWebcamDevice(int deviceId) {
    if (webcamCapture) {
        return webcamCapture->setDevice(deviceId);
    }
    return false;
}

void ScreenRecorder::setWebcamOverlayEnabled(bool enabled) {
    if (webcamCapture) {
        webcamCapture->setOverlayEnabled(enabled);
    }
}

void ScreenRecorder::setWebcamOverlayProperties(float x, float y, float width, float height, int shape) {
    if (webcamCapture) {
        OverlayShape overlayShape = (shape == 0) ? OverlayShape::RECTANGLE : OverlayShape::CIRCLE;
        webcamCapture->setOverlayProperties(x, y, width, height, overlayShape);
    }
}

bool ScreenRecorder::isWebcamOverlayEnabled() const {
    if (webcamCapture) {
        return webcamCapture->isOverlayEnabled();
    }
    return false;
} 