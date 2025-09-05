/**
 * @file src/audio_capture.cpp
 * @brief Implementation of Windows audio capture using WASAPI
 */

#include "../include/audio_capture.h"
#include <iostream>
#include <algorithm>
#include <comdef.h>

// WASAPI constants
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

AudioCapture::AudioCapture() 
    : deviceEnumerator(nullptr)
    , micDevice(nullptr)
    , systemDevice(nullptr)
    , micAudioClient(nullptr)
    , systemAudioClient(nullptr)
    , micCaptureClient(nullptr)
    , systemCaptureClient(nullptr)
    , micFormat(nullptr)
    , systemFormat(nullptr)
    , capturing(false)
    , shouldStop(false)
    , micLevel(0.0f)
    , systemLevel(0.0f) {
}

AudioCapture::~AudioCapture() {
    stop();
    cleanup();
}

bool AudioCapture::initialize() {
    // Try to initialize COM, but don't fail if already initialized
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        std::cerr << "Failed to initialize COM: " << hr << std::endl;
        return false;
    }
    
    // If COM was already initialized in STA mode, we'll work with that
    if (hr == RPC_E_CHANGED_MODE) {
        std::cout << "COM already initialized in STA mode, continuing..." << std::endl;
    }

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL,
                          IID_IMMDeviceEnumerator, (void**)&deviceEnumerator);
    if (FAILED(hr)) {
        std::cerr << "Failed to create device enumerator: " << hr << std::endl;
        return false;
    }

    std::cout << "✓ Audio capture system initialized" << std::endl;
    return true;
}

std::vector<AudioDevice> AudioCapture::enumerateDevices() {
    std::vector<AudioDevice> devices;
    
    if (!deviceEnumerator) {
        return devices;
    }

    // Enumerate input devices (microphones)
    IMMDeviceCollection* inputDevices = nullptr;
    HRESULT hr = deviceEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &inputDevices);
    if (SUCCEEDED(hr)) {
        UINT count = 0;
        inputDevices->GetCount(&count);
        
        for (UINT i = 0; i < count; i++) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(inputDevices->Item(i, &device))) {
                AudioDevice audioDevice;
                
                // Get device ID
                LPWSTR deviceId = nullptr;
                if (SUCCEEDED(device->GetId(&deviceId))) {
                    audioDevice.id = std::string(_bstr_t(deviceId));
                    CoTaskMemFree(deviceId);
                }
                
                // Get device properties
                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        audioDevice.name = std::string(_bstr_t(varName.pwszVal));
                    }
                    
                    PropVariantClear(&varName);
                    props->Release();
                }
                
                audioDevice.isInput = true;
                audioDevice.isDefault = false; // Will be set later for default device
                devices.push_back(audioDevice);
                
                device->Release();
            }
        }
        inputDevices->Release();
    }

    // Enumerate output devices (for system audio loopback)
    IMMDeviceCollection* outputDevices = nullptr;
    hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &outputDevices);
    if (SUCCEEDED(hr)) {
        UINT count = 0;
        outputDevices->GetCount(&count);
        
        for (UINT i = 0; i < count; i++) {
            IMMDevice* device = nullptr;
            if (SUCCEEDED(outputDevices->Item(i, &device))) {
                AudioDevice audioDevice;
                
                // Get device ID
                LPWSTR deviceId = nullptr;
                if (SUCCEEDED(device->GetId(&deviceId))) {
                    audioDevice.id = std::string(_bstr_t(deviceId));
                    CoTaskMemFree(deviceId);
                }
                
                // Get device properties
                IPropertyStore* props = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &props))) {
                    PROPVARIANT varName;
                    PropVariantInit(&varName);
                    
                    if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &varName))) {
                        audioDevice.name = std::string(_bstr_t(varName.pwszVal)) + " (System Audio)";
                    }
                    
                    PropVariantClear(&varName);
                    props->Release();
                }
                
                audioDevice.isInput = false;
                audioDevice.isDefault = false;
                devices.push_back(audioDevice);
                
                device->Release();
            }
        }
        outputDevices->Release();
    }

    // Mark default devices
    IMMDevice* defaultInputDevice = nullptr;
    if (SUCCEEDED(deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &defaultInputDevice))) {
        LPWSTR defaultId = nullptr;
        if (SUCCEEDED(defaultInputDevice->GetId(&defaultId))) {
            std::string defaultIdStr = std::string(_bstr_t(defaultId));
            for (auto& device : devices) {
                if (device.id == defaultIdStr && device.isInput) {
                    device.isDefault = true;
                    break;
                }
            }
            CoTaskMemFree(defaultId);
        }
        defaultInputDevice->Release();
    }

    IMMDevice* defaultOutputDevice = nullptr;
    if (SUCCEEDED(deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &defaultOutputDevice))) {
        LPWSTR defaultId = nullptr;
        if (SUCCEEDED(defaultOutputDevice->GetId(&defaultId))) {
            std::string defaultIdStr = std::string(_bstr_t(defaultId));
            for (auto& device : devices) {
                if (device.id == defaultIdStr && !device.isInput) {
                    device.isDefault = true;
                    break;
                }
            }
            CoTaskMemFree(defaultId);
        }
        defaultOutputDevice->Release();
    }

    std::cout << "✓ Found " << devices.size() << " audio devices" << std::endl;
    return devices;
}

bool AudioCapture::setMicrophoneDevice(const std::string& deviceId) {
    selectedMicDeviceId = deviceId;
    
    if (micDevice) {
        micDevice->Release();
        micDevice = nullptr;
    }

    HRESULT hr;
    if (deviceId.empty()) {
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &micDevice);
    } else {
        hr = deviceEnumerator->GetDevice(_bstr_t(deviceId.c_str()), &micDevice);
    }

    if (FAILED(hr)) {
        std::cerr << "Failed to get microphone device: " << hr << std::endl;
        return false;
    }

    return true;
}

bool AudioCapture::setSystemAudioDevice(const std::string& deviceId) {
    selectedSystemDeviceId = deviceId;
    
    if (systemDevice) {
        systemDevice->Release();
        systemDevice = nullptr;
    }

    HRESULT hr;
    if (deviceId.empty()) {
        hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &systemDevice);
    } else {
        hr = deviceEnumerator->GetDevice(_bstr_t(deviceId.c_str()), &systemDevice);
    }

    if (FAILED(hr)) {
        std::cerr << "Failed to get system audio device: " << hr << std::endl;
        return false;
    }

    return true;
}

bool AudioCapture::start(bool captureMicrophone, bool captureSystemAudio) {
    if (capturing) {
        return true;
    }

    // Set up default devices if none selected
    if (captureMicrophone && !micDevice) {
        setMicrophoneDevice("");
    }
    if (captureSystemAudio && !systemDevice) {
        setSystemAudioDevice("");
    }

    // Initialize devices
    bool micSuccess = true;
    bool systemSuccess = true;

    if (captureMicrophone && micDevice) {
        micSuccess = initializeDevice(micDevice, &micAudioClient, &micCaptureClient, &micFormat, false);
        if (!micSuccess) {
            std::cerr << "Failed to initialize microphone device" << std::endl;
        }
    }

    if (captureSystemAudio && systemDevice) {
        systemSuccess = initializeDevice(systemDevice, &systemAudioClient, &systemCaptureClient, &systemFormat, true);
        if (!systemSuccess) {
            std::cerr << "Failed to initialize system audio device" << std::endl;
        }
    }

    if (!micSuccess && !systemSuccess) {
        std::cerr << "Failed to initialize any audio devices" << std::endl;
        return false;
    }

    // Start capture clients
    bool micStarted = false;
    bool systemStarted = false;
    
    if (micAudioClient) {
        HRESULT hr = micAudioClient->Start();
        if (SUCCEEDED(hr)) {
            micStarted = true;
            std::cout << "✓ Microphone capture started successfully" << std::endl;
        } else {
            std::cerr << "Failed to start microphone capture: 0x" << std::hex << hr << std::dec;
            switch (hr) {
                case 0x87AF0004: std::cerr << " (DEVICE_NOT_READY)"; break;
                case 0x87AF0005: std::cerr << " (DEVICE_INVALIDATED)"; break;
                case 0x80004005: std::cerr << " (ACCESS_DENIED)"; break;
                default: std::cerr << " (UNKNOWN_ERROR)"; break;
            }
            std::cerr << " - continuing without microphone" << std::endl;
        }
    }

    if (systemAudioClient) {
        HRESULT hr = systemAudioClient->Start();
        if (SUCCEEDED(hr)) {
            systemStarted = true;
            std::cout << "✓ System audio capture started successfully" << std::endl;
        } else {
            std::cerr << "Failed to start system audio capture: 0x" << std::hex << hr << std::dec;
            switch (hr) {
                case 0x87AF0004: std::cerr << " (DEVICE_NOT_READY)"; break;
                case 0x87AF0005: std::cerr << " (DEVICE_INVALIDATED)"; break;
                case 0x80004005: std::cerr << " (ACCESS_DENIED)"; break;
                default: std::cerr << " (UNKNOWN_ERROR)"; break;
            }
            std::cerr << " - continuing without system audio" << std::endl;
        }
    }
    
    if (!micStarted && !systemStarted) {
        std::cerr << "No audio devices started successfully" << std::endl;
        return true; // Still continue, just without audio
    }

    // Start capture thread
    capturing = true;
    shouldStop = false;
    captureThread = std::thread(&AudioCapture::captureThreadProc, this);

    std::cout << "✓ Audio capture started" << std::endl;
    return true;
}

void AudioCapture::stop() {
    if (!capturing) {
        return;
    }

    shouldStop = true;
    if (captureThread.joinable()) {
        captureThread.join();
    }

    // Stop audio clients
    if (micAudioClient) {
        micAudioClient->Stop();
    }
    if (systemAudioClient) {
        systemAudioClient->Stop();
    }

    capturing = false;
    std::cout << "✓ Audio capture stopped" << std::endl;
}

bool AudioCapture::getNextSample(AudioSample& sample) {
    std::lock_guard<std::mutex> lock(sampleMutex);
    if (sampleQueue.empty()) {
        return false;
    }

    sample = std::move(sampleQueue.front());
    sampleQueue.erase(sampleQueue.begin());
    return true;
}

void AudioCapture::captureThreadProc() {
    while (!shouldStop) {
        bool hasData = false;

        // Capture microphone data
        if (micCaptureClient) {
            UINT32 numFrames = 0;
            HRESULT hr = micCaptureClient->GetNextPacketSize(&numFrames);
            
            if (SUCCEEDED(hr) && numFrames > 0) {
                BYTE* data = nullptr;
                DWORD flags = 0;
                
                hr = micCaptureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
                if (SUCCEEDED(hr)) {
                    AudioSample sample;
                    sample.sampleRate = micFormat->nSamplesPerSec;
                    sample.channels = micFormat->nChannels;
                    sample.source = AudioSample::Source::Microphone;
                    sample.timestamp = std::chrono::steady_clock::now();

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        // Push silence to keep timing consistent
                        sample.data.assign(numFrames * micFormat->nChannels, 0.0f);
                        micLevel = 0.0f;
                    } else {
                        convertSamplesToFloat(data, numFrames, micFormat, sample.data);
                        micLevel = calculateLevel(data, numFrames, micFormat);
                    }

                    std::lock_guard<std::mutex> lock(sampleMutex);
                    if (sampleQueue.size() < MAX_QUEUE_SIZE) {
                        sampleQueue.push_back(std::move(sample));
                        hasData = true;
                    }
                    
                    micCaptureClient->ReleaseBuffer(numFrames);
                }
            }
        }

        // Capture system audio data
        if (systemCaptureClient) {
            UINT32 numFrames = 0;
            HRESULT hr = systemCaptureClient->GetNextPacketSize(&numFrames);
            
            if (SUCCEEDED(hr) && numFrames > 0) {
                BYTE* data = nullptr;
                DWORD flags = 0;
                
                hr = systemCaptureClient->GetBuffer(&data, &numFrames, &flags, nullptr, nullptr);
                if (SUCCEEDED(hr)) {
                    AudioSample sample;
                    sample.sampleRate = systemFormat->nSamplesPerSec;
                    sample.channels = systemFormat->nChannels;
                    sample.source = AudioSample::Source::System;
                    sample.timestamp = std::chrono::steady_clock::now();

                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        sample.data.assign(numFrames * systemFormat->nChannels, 0.0f);
                        systemLevel = 0.0f;
                    } else {
                        convertSamplesToFloat(data, numFrames, systemFormat, sample.data);
                        systemLevel = calculateLevel(data, numFrames, systemFormat);
                    }

                    std::lock_guard<std::mutex> lock(sampleMutex);
                    if (sampleQueue.size() < MAX_QUEUE_SIZE) {
                        sampleQueue.push_back(std::move(sample));
                        hasData = true;
                    }
                    
                    systemCaptureClient->ReleaseBuffer(numFrames);
                }
            }
        }

        if (!hasData) {
            Sleep(5); // Small delay if no data available
        }
    }
}

bool AudioCapture::initializeDevice(IMMDevice* device, IAudioClient** audioClient,
                                   IAudioCaptureClient** captureClient, WAVEFORMATEX** format,
                                   bool isLoopback) {
    HRESULT hr = device->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void**)audioClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate audio client: " << hr << std::endl;
        return false;
    }

    hr = (*audioClient)->GetMixFormat(format);
    if (FAILED(hr)) {
        std::cerr << "Failed to get mix format: " << hr << std::endl;
        return false;
    }

    DWORD flags = 0;
    if (isLoopback) {
        flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
    }

    hr = (*audioClient)->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 10000000, 0, *format, nullptr);
    if (FAILED(hr)) {
        std::cerr << "Failed to initialize audio client: " << hr << std::endl;
        return false;
    }

    hr = (*audioClient)->GetService(IID_IAudioCaptureClient, (void**)captureClient);
    if (FAILED(hr)) {
        std::cerr << "Failed to get capture client: " << hr << std::endl;
        return false;
    }

    return true;
}

float AudioCapture::calculateLevel(const BYTE* samples, UINT32 numSamples, const WAVEFORMATEX* format) {
    if (!samples || numSamples == 0 || !format) {
        return 0.0f;
    }

    float sum = 0.0f;
    
    if (format->wBitsPerSample == 16) {
        const int16_t* int16Samples = reinterpret_cast<const int16_t*>(samples);
        for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
            float sample = int16Samples[i] / 32768.0f;
            sum += sample * sample;
        }
    } else if (format->wBitsPerSample == 32) {
        if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            const float* floatSamples = reinterpret_cast<const float*>(samples);
            for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
                sum += floatSamples[i] * floatSamples[i];
            }
        } else {
            const int32_t* int32Samples = reinterpret_cast<const int32_t*>(samples);
            for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
                float sample = int32Samples[i] / 2147483648.0f;
                sum += sample * sample;
            }
        }
    }

    float rms = sqrt(sum / (numSamples * format->nChannels));
    return (rms > 1.0f) ? 1.0f : rms; // Clamp to avoid macro conflicts
}

void AudioCapture::convertSamplesToFloat(const BYTE* samples, UINT32 numSamples,
                                        const WAVEFORMATEX* format, std::vector<float>& output) {
    output.clear();
    output.reserve(numSamples * format->nChannels);

    if (format->wBitsPerSample == 16) {
        const int16_t* int16Samples = reinterpret_cast<const int16_t*>(samples);
        for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
            output.push_back(int16Samples[i] / 32768.0f);
        }
    } else if (format->wBitsPerSample == 32) {
        if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
            const float* floatSamples = reinterpret_cast<const float*>(samples);
            for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
                output.push_back(floatSamples[i]);
            }
        } else {
            const int32_t* int32Samples = reinterpret_cast<const int32_t*>(samples);
            for (UINT32 i = 0; i < numSamples * format->nChannels; i++) {
                output.push_back(int32Samples[i] / 2147483648.0f);
            }
        }
    }
}

void AudioCapture::cleanup() {
    if (micCaptureClient) {
        micCaptureClient->Release();
        micCaptureClient = nullptr;
    }
    if (systemCaptureClient) {
        systemCaptureClient->Release();
        systemCaptureClient = nullptr;
    }
    if (micAudioClient) {
        micAudioClient->Release();
        micAudioClient = nullptr;
    }
    if (systemAudioClient) {
        systemAudioClient->Release();
        systemAudioClient = nullptr;
    }
    if (micDevice) {
        micDevice->Release();
        micDevice = nullptr;
    }
    if (systemDevice) {
        systemDevice->Release();
        systemDevice = nullptr;
    }
    if (deviceEnumerator) {
        deviceEnumerator->Release();
        deviceEnumerator = nullptr;
    }
    if (micFormat) {
        CoTaskMemFree(micFormat);
        micFormat = nullptr;
    }
    if (systemFormat) {
        CoTaskMemFree(systemFormat);
        systemFormat = nullptr;
    }
}
