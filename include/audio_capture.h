/**
 * @file include/audio_capture.h
 * @brief Windows audio capture using WASAPI for system and microphone audio
 */

#pragma once

#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <functiondiscoverykeys_devpkey.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

/**
 * @brief Audio device information
 */
struct AudioDevice {
    std::string id;
    std::string name;
    bool isDefault;
    bool isInput;  // true for microphones, false for output devices (system audio)
};

/**
 * @brief Audio sample data
 */
struct AudioSample {
    std::vector<float> data;
    int sampleRate;
    int channels;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief WASAPI audio capture class for recording system and microphone audio
 */
class AudioCapture {
public:
    AudioCapture();
    ~AudioCapture();

    /**
     * @brief Initialize the audio capture system
     * @return true if successful
     */
    bool initialize();

    /**
     * @brief Enumerate available audio devices
     * @return vector of available audio devices
     */
    std::vector<AudioDevice> enumerateDevices();

    /**
     * @brief Set the microphone device to use
     * @param deviceId the device ID, or empty string for default
     * @return true if successful
     */
    bool setMicrophoneDevice(const std::string& deviceId = "");

    /**
     * @brief Set the system audio device to use (for loopback capture)
     * @param deviceId the device ID, or empty string for default
     * @return true if successful
     */
    bool setSystemAudioDevice(const std::string& deviceId = "");

    /**
     * @brief Start capturing audio
     * @param captureMicrophone enable microphone capture
     * @param captureSystemAudio enable system audio capture
     * @return true if successful
     */
    bool start(bool captureMicrophone = true, bool captureSystemAudio = true);

    /**
     * @brief Stop capturing audio
     */
    void stop();

    /**
     * @brief Check if audio capture is active
     * @return true if capturing
     */
    bool isCapturing() const { return capturing; }

    /**
     * @brief Get the next audio sample (blocking call)
     * @param sample output parameter for the audio sample
     * @return true if a sample was retrieved
     */
    bool getNextSample(AudioSample& sample);

    /**
     * @brief Get current microphone level (0.0 to 1.0)
     * @return microphone level
     */
    float getMicrophoneLevel() const { return micLevel; }

    /**
     * @brief Get current system audio level (0.0 to 1.0)
     * @return system audio level
     */
    float getSystemAudioLevel() const { return systemLevel; }

private:
    // WASAPI interfaces
    IMMDeviceEnumerator* deviceEnumerator;
    IMMDevice* micDevice;
    IMMDevice* systemDevice;
    IAudioClient* micAudioClient;
    IAudioClient* systemAudioClient;
    IAudioCaptureClient* micCaptureClient;
    IAudioCaptureClient* systemCaptureClient;

    // Audio format
    WAVEFORMATEX* micFormat;
    WAVEFORMATEX* systemFormat;

    // Threading
    std::thread captureThread;
    std::atomic<bool> capturing;
    std::atomic<bool> shouldStop;

    // Audio data
    mutable std::mutex sampleMutex;
    std::vector<AudioSample> sampleQueue;
    static const size_t MAX_QUEUE_SIZE = 100;

    // Audio levels
    std::atomic<float> micLevel;
    std::atomic<float> systemLevel;

    // Device selection
    std::string selectedMicDeviceId;
    std::string selectedSystemDeviceId;

    /**
     * @brief Audio capture thread function
     */
    void captureThreadProc();

    /**
     * @brief Initialize a specific audio device
     * @param device the device to initialize
     * @param audioClient output audio client
     * @param captureClient output capture client
     * @param format output audio format
     * @param isLoopback true for system audio loopback
     * @return true if successful
     */
    bool initializeDevice(IMMDevice* device, IAudioClient** audioClient, 
                         IAudioCaptureClient** captureClient, WAVEFORMATEX** format,
                         bool isLoopback = false);

    /**
     * @brief Calculate audio level from samples
     * @param samples pointer to audio samples
     * @param numSamples number of samples
     * @param format audio format
     * @return audio level (0.0 to 1.0)
     */
    float calculateLevel(const BYTE* samples, UINT32 numSamples, const WAVEFORMATEX* format);

    /**
     * @brief Convert audio samples to float format
     * @param samples input samples
     * @param numSamples number of samples
     * @param format audio format
     * @param output output vector
     */
    void convertSamplesToFloat(const BYTE* samples, UINT32 numSamples, 
                              const WAVEFORMATEX* format, std::vector<float>& output);

    /**
     * @brief Release WASAPI resources
     */
    void cleanup();
};
