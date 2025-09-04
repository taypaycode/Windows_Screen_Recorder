/**
 * File: include/imgui_ui.h
 * Purpose: High-performance Dear ImGui UI for ScreenIT with gaming aesthetics
 */

#pragma once

#include <windows.h>
#include <gl/GL.h>
#include <string>
#include <memory>

// Dear ImGui headers
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_opengl3.h"

// Forward declarations
class ScreenRecorder;
enum class VideoCodec;
enum class QualityPreset;

class ImGuiUI {
public:
    ImGuiUI();
    ~ImGuiUI();

    // Initialize and run the UI
    bool initialize(HINSTANCE hInstance);
    int run();
    void shutdown();

    // Connect to recording backend
    void setScreenRecorder(ScreenRecorder* recorder);

private:
    // Window management
    static LRESULT WINAPI WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    bool createWindow(HINSTANCE hInstance);
    bool initializeOpenGL();
    bool initializeImGui();

    // UI rendering
    void renderFrame();
    void renderMainInterface();
    void renderRecordingControls();
    void renderSettingsPanel();
    void renderStatusBar();
    void applyGamingTheme();

    // Event handling
    void handleStartRecording();
    void handleStopRecording();
    void handlePauseRecording();

    // Utility functions
    std::string formatTime(int seconds);
    std::string formatFileSize(float megabytes);
    const char* codecToString(VideoCodec codec);
    const char* qualityToString(QualityPreset quality);

private:
    // Window handles
    HWND m_hwnd;
    HDC m_hdc;
    HGLRC m_hglrc;
    
    // ImGui state
    bool m_showDemo;
    bool m_showSettings;
    
    // Recording backend
    ScreenRecorder* m_recorder;
    
    // UI state
    struct UIState {
        bool isRecording = false;
        bool isPaused = false;
        int duration = 0;
        float fileSize = 0.0f;
        std::string outputFile;
        VideoCodec currentCodec;
        QualityPreset currentQuality;
        int fps = 30;
    } m_state;
    
    // Gaming theme colors
    struct GamingColors {
        ImVec4 neonBlue = ImVec4(0.0f, 0.96f, 1.0f, 1.0f);
        ImVec4 neonPink = ImVec4(1.0f, 0.0f, 0.43f, 1.0f);
        ImVec4 neonGreen = ImVec4(0.22f, 1.0f, 0.08f, 1.0f);
        ImVec4 neonPurple = ImVec4(0.75f, 0.0f, 1.0f, 1.0f);
        ImVec4 neonOrange = ImVec4(1.0f, 0.53f, 0.0f, 1.0f);
        ImVec4 pixelDark = ImVec4(0.10f, 0.10f, 0.18f, 1.0f);
        ImVec4 pixelNavy = ImVec4(0.06f, 0.20f, 0.38f, 1.0f);
        ImVec4 arcadeRed = ImVec4(1.0f, 0.28f, 0.34f, 1.0f);
    } m_colors;
    
    // Window properties
    static constexpr int WINDOW_WIDTH = 800;
    static constexpr int WINDOW_HEIGHT = 600;
    static constexpr const char* WINDOW_TITLE = "🎮 ScreenIT Arcade - High Performance Recording";
};
