/**
 * @file include/webcam_capture.h
 * @brief Webcam capture using OpenCV with overlay support
 */

#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>

/**
 * @brief Webcam device information
 */
struct WebcamDevice {
    int id;
    std::string name;
    bool isDefault;
    int width;
    int height;
    double fps;
};

/**
 * @brief Overlay shape types
 */
enum class OverlayShape {
    RECTANGLE,
    CIRCLE
};

/**
 * @brief Webcam capture class with overlay support
 */
class WebcamCapture {
public:
    WebcamCapture();
    ~WebcamCapture();

    /**
     * @brief Initialize the webcam capture system
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Enumerate available webcam devices
     * @return vector of available webcam devices
     */
    std::vector<WebcamDevice> enumerateDevices();

    /**
     * @brief Set the webcam device to use
     * @param deviceId the device ID, or -1 for default
     * @return true if successful
     */
    bool setDevice(int deviceId = -1);

    /**
     * @brief Start capturing webcam video
     * @param width desired width (0 for default)
     * @param height desired height (0 for default) 
     * @param fps desired frame rate (0 for default)
     * @return true if successful
     */
    bool start(int width = 0, int height = 0, double fps = 0);

    /**
     * @brief Stop capturing webcam video
     */
    void stop();

    /**
     * @brief Check if webcam capture is active
     * @return true if capturing
     */
    bool isCapturing() const { return capturing; }

    /**
     * @brief Get the current webcam frame
     * @param frame output parameter for the webcam frame
     * @return true if a frame was retrieved
     */
    bool getCurrentFrame(cv::Mat& frame);

    /**
     * @brief Apply webcam overlay to a screen frame
     * @param screenFrame the screen frame to overlay onto
     * @return true if overlay was applied
     */
    bool applyOverlay(cv::Mat& screenFrame) const;

    /**
     * @brief Set overlay properties
     * @param x X position (0-1 normalized)
     * @param y Y position (0-1 normalized)
     * @param width overlay width (0-1 normalized)
     * @param height overlay height (0-1 normalized)
     * @param shape overlay shape
     */
    void setOverlayProperties(float x, float y, float width, float height, OverlayShape shape);

    /**
     * @brief Enable or disable overlay
     * @param enabled true to enable overlay
     */
    void setOverlayEnabled(bool enabled) { overlayEnabled = enabled; }

    /**
     * @brief Check if overlay is enabled
     * @return true if overlay is enabled
     */
    bool isOverlayEnabled() const { return overlayEnabled; }

    /**
     * @brief Get current overlay position (normalized coordinates)
     */
    void getOverlayPosition(float& x, float& y) const { x = overlayX; y = overlayY; }

    /**
     * @brief Get current overlay size (normalized coordinates)
     */
    void getOverlaySize(float& width, float& height) const { width = overlayWidth; height = overlayHeight; }

    /**
     * @brief Get current overlay shape
     */
    OverlayShape getOverlayShape() const { return overlayShape; }

    /**
     * @brief Get current device information
     */
    WebcamDevice getCurrentDevice() const { return currentDevice; }

private:
    // OpenCV video capture
    cv::VideoCapture capture;
    
    // Device management
    WebcamDevice currentDevice;
    std::vector<WebcamDevice> availableDevices;
    
    // Capture state
    std::atomic<bool> capturing;
    std::atomic<bool> overlayEnabled;
    std::atomic<bool> stopRequested;
    std::thread captureThread;
    
    // Current frame
    mutable std::mutex frameMutex;
    cv::Mat currentFrame;
    
    // Overlay properties (normalized coordinates 0-1)
    mutable std::mutex overlayMutex;
    std::atomic<float> overlayX;
    std::atomic<float> overlayY;
    std::atomic<float> overlayWidth;
    std::atomic<float> overlayHeight;
    std::atomic<OverlayShape> overlayShape;
    
    /**
     * @brief Test a webcam device to get its capabilities
     * @param deviceId the device ID to test
     * @return device information
     */
    WebcamDevice testDevice(int deviceId);

    /**
     * @brief Create a circular mask for overlay
     * @param size the size of the mask
     * @return mask matrix
     */
    cv::Mat createCircleMask(cv::Size size) const;

    /**
     * @brief Background loop that continually grabs webcam frames
     */
    void captureLoop();
};
