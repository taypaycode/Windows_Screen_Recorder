#include "../include/screen_recorder.h"
#include "../include/imgui_ui.h"
#include <windows.h>
#include <iostream>
#include <string>
#include <map>
#include <thread>
#include <chrono>

// Command line argument parser
std::map<std::string, std::string> parseArgs(int argc, char* argv[]) {
    std::map<std::string, std::string> args;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.substr(0, 2) == "--") {
            std::string key = arg.substr(2);
            if (i + 1 < argc && std::string(argv[i + 1]).substr(0, 2) != "--") {
                args[key] = argv[i + 1];
                i++; // Skip the value
            } else {
                args[key] = "true"; // Flag without value
            }
        }
    }
    return args;
}

// CLI mode for Electron integration
int runCliMode(const std::map<std::string, std::string>& args) {
    std::cout << "🎮 ScreenIT Arcade - CLI Mode" << std::endl;
    
    ScreenRecorder recorder;
    
    // Parse settings from command line
    std::string codec = args.count("codec") ? args.at("codec") : "H264_HARDWARE";
    std::string quality = args.count("quality") ? args.at("quality") : "SMALL_SHARP"; 
    int fps = args.count("fps") ? std::stoi(args.at("fps")) : 30;
    std::string output = args.count("output") ? args.at("output") : "recording.mp4";
    
    std::cout << "📹 Starting recording with:" << std::endl;
    std::cout << "  - Codec: " << codec << std::endl;
    std::cout << "  - Quality: " << quality << std::endl;
    std::cout << "  - FPS: " << fps << std::endl;
    std::cout << "  - Output: " << output << std::endl;
    
    // Set codec
    if (codec == "H264_HARDWARE") recorder.setCodec(VideoCodec::H264_HARDWARE);
    else if (codec == "H264_SOFTWARE") recorder.setCodec(VideoCodec::H264_SOFTWARE);
    else if (codec == "HEVC_HARDWARE") recorder.setCodec(VideoCodec::HEVC_HARDWARE);
    else if (codec == "HEVC_SOFTWARE") recorder.setCodec(VideoCodec::HEVC_SOFTWARE);
    
    // Set quality
    if (quality == "SMALL_SHARP") recorder.setQuality(QualityPreset::SMALL_SHARP);
    else if (quality == "BALANCED") recorder.setQuality(QualityPreset::BALANCED);
    else if (quality == "HIGH_QUALITY") recorder.setQuality(QualityPreset::HIGH_QUALITY);
    else if (quality == "LOSSLESS") recorder.setQuality(QualityPreset::LOSSLESS);
    
    recorder.setFrameRate(fps);
    
    // Start recording
    if (!recorder.start(output)) {
        std::cerr << "❌ Failed to start recording!" << std::endl;
        return 1;
    }
    
    std::cout << "🔴 Recording started! Press Ctrl+C to stop..." << std::endl;
    
    // Keep recording until interrupted
    // In practice, Electron will manage the process lifecycle
    while (recorder.isRecording()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "✅ Recording completed!" << std::endl;
    return 0;
}

// Console entry point for CLI mode
int main(int argc, char* argv[]) {
    auto args = parseArgs(argc, argv);
    
    // Only handle CLI mode here
    if (args.count("cli") || args.count("codec") || args.count("output")) {
        return runCliMode(args);
    }
    
    // For GUI mode, redirect to WinMain
    return 0;
}

// Windows entry point for GUI mode
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line if any
    std::map<std::string, std::string> args;
    if (lpCmdLine && strlen(lpCmdLine) > 0) {
        // Simple parsing for WinMain - could be enhanced
        std::string cmdLine(lpCmdLine);
        if (cmdLine.find("--cli") != std::string::npos) {
            // Fall back to console mode
            AllocConsole();
            freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
            freopen_s((FILE**)stderr, "CONOUT$", "w", stderr);
            freopen_s((FILE**)stdin, "CONIN$", "r", stdin);
        }
    }
    
    // GUI mode - High-performance ImGui interface
    std::cout << "=== ScreenIT Arcade - High Performance GUI Mode ===" << std::endl;
    std::cout << "🎮 Loading Dear ImGui gaming interface..." << std::endl;
    
    // Create the screen recorder  
    ScreenRecorder recorder;
    
    // Create and initialize the ImGui UI
    ImGuiUI ui;
    ui.setScreenRecorder(&recorder);
    
    if (!ui.initialize(hInstance)) {
        std::cerr << "❌ Failed to initialize high-performance UI!" << std::endl;
        return 1;
    }
    
    std::cout << "✅ High-performance gaming interface loaded successfully!" << std::endl;
    
    // Run the ultra-fast ImGui render loop
    return ui.run();
}

// Original WinMain for production
/*
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Create the screen recorder
    ScreenRecorder recorder;
    
    // Create and initialize the UI
    ScreenUI ui;
    ui.setScreenRecorder(&recorder);
    
    if (!ui.init(hInstance)) {
        MessageBox(NULL, L"Failed to initialize the application!", L"Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Run the UI message loop
    return ui.run();
}
*/ 