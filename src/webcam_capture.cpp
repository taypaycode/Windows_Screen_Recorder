/**
 * @file src/webcam_capture.cpp
 * @brief Implementation of webcam capture with overlay support
 */

#include "../include/webcam_capture.h"
#include <iostream>
#include <algorithm>

WebcamCapture::WebcamCapture()
    : capturing(false)
    , overlayEnabled(false)
    , overlayX(0.8f)
    , overlayY(0.1f) 
    , overlayWidth(0.15f)
    , overlayHeight(0.2f)
    , overlayShape(OverlayShape::RECTANGLE)
    , stopRequested(false) {
    currentDevice.id = -1;
}

WebcamCapture::~WebcamCapture() {
    stop();
}

bool WebcamCapture::initialize() {
    std::cout << "✓ Webcam capture system initialized" << std::endl;
    return true;
}

std::vector<WebcamDevice> WebcamCapture::enumerateDevices() {
    availableDevices.clear();
    
    // Test first few device indices to find available cameras
    // Limit to reasonable range to avoid long delays
    for (int i = 0; i < 5; i++) {
        WebcamDevice device = testDevice(i);
        if (device.id != -1) {
            availableDevices.push_back(device);
        }
    }
    
    // Mark first device as default if any found
    if (!availableDevices.empty()) {
        availableDevices[0].isDefault = true;
    }
    
    std::cout << "✓ Found " << availableDevices.size() << " webcam devices" << std::endl;
    return availableDevices;
}

WebcamDevice WebcamCapture::testDevice(int deviceId) {
    WebcamDevice device;
    device.id = -1; // Invalid by default
    device.isDefault = false;
    
    cv::VideoCapture testCapture;
    
    // Use Media Foundation backend on Windows for better compatibility
#ifdef _WIN32
    if (!testCapture.open(deviceId, cv::CAP_MSMF)) {
        return device;
    }
#else
    if (!testCapture.open(deviceId)) {
        return device;
    }
#endif
    
    if (!testCapture.isOpened()) {
        return device;
    }
    
    // Get device capabilities
    device.id = deviceId;
    device.width = static_cast<int>(testCapture.get(cv::CAP_PROP_FRAME_WIDTH));
    device.height = static_cast<int>(testCapture.get(cv::CAP_PROP_FRAME_HEIGHT));
    device.fps = testCapture.get(cv::CAP_PROP_FPS);
    
    // Create a device name
    device.name = "Camera " + std::to_string(deviceId);
    if (device.width > 0 && device.height > 0) {
        device.name += " (" + std::to_string(device.width) + "x" + std::to_string(device.height) + ")";
    }
    
    testCapture.release();
    return device;
}

bool WebcamCapture::setDevice(int deviceId) {
    if (capturing) {
        std::cerr << "Cannot change device while capturing" << std::endl;
        return false;
    }
    
    if (deviceId == -1) {
        // Find first available device
        auto devices = enumerateDevices();
        if (devices.empty()) {
            std::cerr << "No webcam devices found" << std::endl;
            return false;
        }
        deviceId = devices[0].id;
    }
    
    // Test the device
    currentDevice = testDevice(deviceId);
    if (currentDevice.id == -1) {
        std::cerr << "Failed to set webcam device " << deviceId << std::endl;
        return false;
    }
    
    std::cout << "✓ Webcam device set to: " << currentDevice.name << std::endl;
    return true;
}

bool WebcamCapture::start(int width, int height, double fps) {
    if (capturing) {
        return true;
    }
    
    if (currentDevice.id == -1) {
        if (!setDevice(-1)) {
            return false;
        }
    }
    
    // Open the capture device
#ifdef _WIN32
    if (!capture.open(currentDevice.id, cv::CAP_MSMF)) {
        std::cerr << "Failed to open webcam device " << currentDevice.id << std::endl;
        return false;
    }
#else
    if (!capture.open(currentDevice.id)) {
        std::cerr << "Failed to open webcam device " << currentDevice.id << std::endl;
        return false;
    }
#endif
    
    // Set desired properties if specified
    if (width > 0) {
        capture.set(cv::CAP_PROP_FRAME_WIDTH, width);
    }
    if (height > 0) {
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    }
    if (fps > 0) {
        capture.set(cv::CAP_PROP_FPS, fps);
    }
    
    // Verify the capture is working
    cv::Mat testFrame;
    if (!capture.read(testFrame) || testFrame.empty()) {
        std::cerr << "Failed to read from webcam device" << std::endl;
        capture.release();
        return false;
    }
    
    // Store the first frame
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        currentFrame = testFrame.clone();
    }
    
    capturing = true;
    stopRequested = false;
    std::cout << "✓ Webcam capture started: " << currentDevice.name << std::endl;
    // Start background capture loop to keep frames fresh
    captureThread = std::thread(&WebcamCapture::captureLoop, this);
    return true;
}

void WebcamCapture::stop() {
    if (!capturing) {
        return;
    }
    
    capturing = false;
    stopRequested = true;
    if (captureThread.joinable()) {
        captureThread.join();
    }
    if (capture.isOpened()) {
        capture.release();
    }
    
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        currentFrame.release();
    }
    
    std::cout << "✓ Webcam capture stopped" << std::endl;
}

bool WebcamCapture::getCurrentFrame(cv::Mat& frame) {
    if (!capturing || !capture.isOpened()) {
        return false;
    }
    
    cv::Mat newFrame;
    if (!capture.read(newFrame) || newFrame.empty()) {
        return false;
    }
    
    // Update stored frame
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        currentFrame = newFrame.clone();
        frame = newFrame.clone();
    }
    
    return true;
}

bool WebcamCapture::applyOverlay(cv::Mat& screenFrame) const {
    if (!overlayEnabled) {
        // std::cout << "Webcam overlay disabled" << std::endl;
        return false;
    }
    if (!capturing) {
        std::cout << "Webcam not capturing" << std::endl;
        return false;
    }
    
    cv::Mat webcamFrame;
    
    // Get the current cached frame (background loop keeps this fresh)
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (currentFrame.empty()) {
            return false;
        }
        webcamFrame = currentFrame.clone();
    }
    
    // Calculate overlay position and size in screen frame coordinates
    std::lock_guard<std::mutex> overlayLock(overlayMutex);
    int overlayPixelX = static_cast<int>(overlayX * screenFrame.cols);
    int overlayPixelY = static_cast<int>(overlayY * screenFrame.rows);
    int overlayPixelWidth = static_cast<int>(overlayWidth * screenFrame.cols);
    int overlayPixelHeight = static_cast<int>(overlayHeight * screenFrame.rows);
    
    // Ensure overlay stays within screen bounds
    overlayPixelX = std::max(0, std::min(overlayPixelX, screenFrame.cols - overlayPixelWidth));
    overlayPixelY = std::max(0, std::min(overlayPixelY, screenFrame.rows - overlayPixelHeight));
    overlayPixelWidth = std::min(overlayPixelWidth, screenFrame.cols - overlayPixelX);
    overlayPixelHeight = std::min(overlayPixelHeight, screenFrame.rows - overlayPixelY);
    
    if (overlayPixelWidth <= 0 || overlayPixelHeight <= 0) {
        return false;
    }
    
    // Resize webcam frame to overlay size
    cv::Mat resizedWebcam;
    cv::resize(webcamFrame, resizedWebcam, cv::Size(overlayPixelWidth, overlayPixelHeight));
    
    // Define the region of interest in the screen frame
    cv::Rect overlayRect(overlayPixelX, overlayPixelY, overlayPixelWidth, overlayPixelHeight);
    cv::Mat overlayRegion = screenFrame(overlayRect);
    
    if (overlayShape == OverlayShape::CIRCLE) {
        // Create circular mask
        cv::Mat mask = createCircleMask(resizedWebcam.size());
        
        // Apply the webcam frame with circular mask
        resizedWebcam.copyTo(overlayRegion, mask);
    } else {
        // Simple rectangular overlay
        resizedWebcam.copyTo(overlayRegion);
    }
    
    return true;
}

void WebcamCapture::captureLoop() {
    // Keep grabbing frames at ~30fps (best effort)
    while (!stopRequested) {
        if (!capture.isOpened()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        cv::Mat frame;
        if (capture.read(frame) && !frame.empty()) {
            std::lock_guard<std::mutex> lock(frameMutex);
            currentFrame = frame.clone();
        } else {
            // If reading fails, wait briefly
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

void WebcamCapture::setOverlayProperties(float x, float y, float width, float height, OverlayShape shape) {
    std::lock_guard<std::mutex> lock(overlayMutex);
    
    // Clamp values to valid range
    overlayX = std::max(0.0f, std::min(1.0f, x));
    overlayY = std::max(0.0f, std::min(1.0f, y));
    overlayWidth = std::max(0.01f, std::min(1.0f, width));
    overlayHeight = std::max(0.01f, std::min(1.0f, height));
    overlayShape = shape;
}

cv::Mat WebcamCapture::createCircleMask(cv::Size size) const {
    cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);
    
    cv::Point center(size.width / 2, size.height / 2);
    int radius = std::min(size.width, size.height) / 2;
    
    cv::circle(mask, center, radius, cv::Scalar(255), -1);
    
    return mask;
}
