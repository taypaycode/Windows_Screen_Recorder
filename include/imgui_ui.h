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
    void applyModernTheme();

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
    
    // Modern professional colors
    struct ModernColors {
        ImVec4 white = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImVec4 lightGray = ImVec4(0.96f, 0.96f, 0.96f, 1.0f);
        ImVec4 mediumGray = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
        ImVec4 darkGray = ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        ImVec4 charcoal = ImVec4(0.25f, 0.25f, 0.25f, 1.0f);
        ImVec4 accent = ImVec4(1.0f, 0.80f, 0.20f, 1.0f);       // Subtle yellow accent
        ImVec4 accentHover = ImVec4(1.0f, 0.75f, 0.10f, 1.0f);  // Darker yellow on hover
        ImVec4 success = ImVec4(0.20f, 0.75f, 0.30f, 1.0f);     // Clean green
        ImVec4 recording = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);   // Professional red
        ImVec4 shadow = ImVec4(0.0f, 0.0f, 0.0f, 0.1f);         // Subtle shadows
    } m_colors;
    
    // Window properties
    static constexpr int WINDOW_WIDTH = 800;
    static constexpr int WINDOW_HEIGHT = 600;
    static constexpr const char* WINDOW_TITLE = "ScreenIT Pro - Professional Screen Recording";
};
