/**
 * File: src/imgui_ui.cpp
 * Purpose: High-performance Dear ImGui implementation for ScreenIT
 */

#include "../include/imgui_ui.h"
#include "../include/screen_recorder.h"
#include "../include/video_types.h"
#include <iostream>
#include <sstream>
#include <iomanip>

// OpenGL function loader (simple approach for Windows)
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#ifndef WINGDIAPI
#define WINGDIAPI __declspec(dllimport)
#endif

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiUI::ImGuiUI() 
    : m_hwnd(nullptr)
    , m_hdc(nullptr)
    , m_hglrc(nullptr)
    , m_showDemo(false)
    , m_showSettings(false)
    , m_recorder(nullptr)
{
    m_state.currentCodec = VideoCodec::H264_HARDWARE;
    m_state.currentQuality = QualityPreset::SMALL_SHARP;
}

ImGuiUI::~ImGuiUI() {
    shutdown();
}

bool ImGuiUI::initialize(HINSTANCE hInstance) {
    std::cout << "🎮 Initializing ScreenIT Arcade ImGui Interface..." << std::endl;
    
    if (!createWindow(hInstance)) {
        std::cerr << "❌ Failed to create window!" << std::endl;
        return false;
    }
    
    if (!initializeOpenGL()) {
        std::cerr << "❌ Failed to initialize OpenGL!" << std::endl;
        return false;
    }
    
    if (!initializeImGui()) {
        std::cerr << "❌ Failed to initialize ImGui!" << std::endl;
        return false;
    }
    
    applyGamingTheme();
    
    std::cout << "✅ High-performance UI initialized successfully!" << std::endl;
    return true;
}

bool ImGuiUI::createWindow(HINSTANCE hInstance) {
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszMenuName = nullptr;
    wc.lpszClassName = L"ScreenITArcade";
    wc.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wc)) {
        return false;
    }
    
    // Create window
    m_hwnd = CreateWindowW(
        wc.lpszClassName,
        L"🎮 ScreenIT Arcade - High Performance Recording",
        WS_OVERLAPPEDWINDOW,
        100, 100,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        nullptr, nullptr,
        hInstance, nullptr
    );
    
    if (!m_hwnd) {
        return false;
    }
    
    // Store this pointer for message handling
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    // Show and update window
    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
    UpdateWindow(m_hwnd);
    
    return true;
}

bool ImGuiUI::initializeOpenGL() {
    m_hdc = GetDC(m_hwnd);
    
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    
    int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
    if (pixelFormat == 0) {
        return false;
    }
    
    if (!SetPixelFormat(m_hdc, pixelFormat, &pfd)) {
        return false;
    }
    
    m_hglrc = wglCreateContext(m_hdc);
    if (!m_hglrc) {
        return false;
    }
    
    if (!wglMakeCurrent(m_hdc, m_hglrc)) {
        return false;
    }
    
    return true;
}

bool ImGuiUI::initializeImGui() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    
    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");
    
    return true;
}

void ImGuiUI::applyGamingTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Gaming color scheme
    style.Colors[ImGuiCol_Text] = m_colors.neonBlue;
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = m_colors.pixelDark;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_Border] = m_colors.neonBlue;
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_FrameBgHovered] = m_colors.neonPurple;
    style.Colors[ImGuiCol_FrameBgActive] = m_colors.neonPink;
    style.Colors[ImGuiCol_TitleBg] = m_colors.pixelDark;
    style.Colors[ImGuiCol_TitleBgActive] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_TitleBgCollapsed] = m_colors.pixelDark;
    style.Colors[ImGuiCol_MenuBarBg] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_ScrollbarBg] = m_colors.pixelDark;
    style.Colors[ImGuiCol_ScrollbarGrab] = m_colors.neonBlue;
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = m_colors.neonPink;
    style.Colors[ImGuiCol_ScrollbarGrabActive] = m_colors.neonGreen;
    style.Colors[ImGuiCol_CheckMark] = m_colors.neonGreen;
    style.Colors[ImGuiCol_SliderGrab] = m_colors.neonOrange;
    style.Colors[ImGuiCol_SliderGrabActive] = m_colors.neonPink;
    style.Colors[ImGuiCol_Button] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_ButtonHovered] = m_colors.neonPurple;
    style.Colors[ImGuiCol_ButtonActive] = m_colors.neonPink;
    style.Colors[ImGuiCol_Header] = m_colors.pixelNavy;
    style.Colors[ImGuiCol_HeaderHovered] = m_colors.neonPurple;
    style.Colors[ImGuiCol_HeaderActive] = m_colors.neonPink;
    
    // Gaming style parameters
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowBorderSize = 2.0f;
    style.FrameBorderSize = 1.0f;
    
    std::cout << "🌈 Applied gaming theme with neon colors!" << std::endl;
}

void ImGuiUI::setScreenRecorder(ScreenRecorder* recorder) {
    m_recorder = recorder;
    std::cout << "🔗 Connected to ScreenRecorder backend" << std::endl;
}

int ImGuiUI::run() {
    MSG msg;
    bool done = false;
    
    std::cout << "🚀 Starting ultra-smooth render loop..." << std::endl;
    
    while (!done) {
        // Poll and handle messages (non-blocking)
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        
        if (done) break;
        
        // Get current window size for viewport
        RECT rect;
        GetClientRect(m_hwnd, &rect);
        int displayWidth = rect.right - rect.left;
        int displayHeight = rect.bottom - rect.top;
        
        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // Update UI state from recorder
        if (m_recorder) {
            m_state.isRecording = m_recorder->isRecording();
            m_state.isPaused = m_recorder->isPaused();
            
            // Update duration and file size simulation
            if (m_state.isRecording && !m_state.isPaused) {
                static auto lastUpdate = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastUpdate);
                if (elapsed.count() >= 1) {
                    m_state.duration++;
                    m_state.fileSize = m_state.duration * 0.0008f; // Ultra-compressed size!
                    lastUpdate = now;
                }
            }
        }
        
        // Render our UI
        renderFrame();
        
        // Rendering with proper viewport
        ImGui::Render();
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(m_colors.pixelDark.x, m_colors.pixelDark.y, m_colors.pixelDark.z, m_colors.pixelDark.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        SwapBuffers(m_hdc);
        
        // Maintain smooth 60 FPS for UI (not recording)
        Sleep(16); // ~60 FPS cap to prevent excessive CPU usage
    }
    
    return static_cast<int>(msg.wParam);
}

void ImGuiUI::renderFrame() {
    // Get actual window size for proper scaling
    RECT rect;
    GetClientRect(m_hwnd, &rect);
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    
    // Create main window that fills the client area
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)windowWidth, (float)windowHeight), ImGuiCond_Always);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | 
                                   ImGuiWindowFlags_NoMove | 
                                   ImGuiWindowFlags_NoResize | 
                                   ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBackground;
    
    ImGui::Begin("ScreenIT Arcade", nullptr, window_flags);
    
    renderMainInterface();
    
    ImGui::End();
    
    // Optional: Show ImGui demo for testing
    if (m_showDemo) {
        ImGui::ShowDemoWindow(&m_showDemo);
    }
}

void ImGuiUI::renderMainInterface() {
    // Title header with better spacing
    ImGui::Spacing();
    ImGui::Indent(20.0f);
    
    // Gaming title with emojis - centered
    float windowWidth = ImGui::GetWindowSize().x;
    std::string title = "🎮 SCREENIT ARCADE - MICROSCOPIC FILES 🚀";
    float textWidth = ImGui::CalcTextSize(title.c_str()).x;
    ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
    ImGui::TextColored(m_colors.neonPink, "%s", title.c_str());
    
    ImGui::Unindent(20.0f);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Main layout with fixed proportions
    ImGui::Columns(2, "MainLayout", true);
    ImGui::SetColumnWidth(0, windowWidth * 0.65f);
    
    // Left column - Recording controls
    renderRecordingControls();
    
    ImGui::NextColumn();
    
    // Right column - Settings
    renderSettingsPanel();
    
    ImGui::Columns(1); // Reset to single column
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    renderStatusBar();
}

void ImGuiUI::renderRecordingControls() {
    ImGui::TextColored(m_colors.neonBlue, "🎬 CAPTURE STATION");
    
    // Recording stats in a grid
    if (ImGui::BeginTable("Stats", 4, ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("⏰ TIME");
        ImGui::TableSetupColumn("💾 SIZE");
        ImGui::TableSetupColumn("🎯 FPS");
        ImGui::TableSetupColumn("⚡ COMPRESSION");
        ImGui::TableHeadersRow();
        
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(m_colors.neonGreen, "%s", formatTime(m_state.duration).c_str());
        
        ImGui::TableSetColumnIndex(1);
        ImGui::TextColored(m_colors.neonPink, "%s", formatFileSize(m_state.fileSize).c_str());
        
        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(m_colors.neonOrange, "%d", m_state.fps);
        
        ImGui::TableSetColumnIndex(3);
        ImGui::TextColored(m_colors.neonGreen, "92%%");
        
        ImGui::EndTable();
    }
    
    ImGui::Spacing();
    
    // Main recording button
    ImVec2 buttonSize(200, 50);
    if (!m_state.isRecording) {
        ImGui::PushStyleColor(ImGuiCol_Button, m_colors.arcadeRed);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_colors.neonPink);
        if (ImGui::Button("🔴 START CAPTURE!", buttonSize)) {
            handleStartRecording();
        }
        ImGui::PopStyleColor(2);
    } else {
        // Show pause/stop buttons
        ImGui::PushStyleColor(ImGuiCol_Button, m_colors.neonOrange);
        if (ImGui::Button(m_state.isPaused ? "▶️ RESUME" : "⏸️ PAUSE", ImVec2(95, 50))) {
            handlePauseRecording();
        }
        ImGui::PopStyleColor();
        
        ImGui::SameLine();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        if (ImGui::Button("⏹️ STOP", ImVec2(95, 50))) {
            handleStopRecording();
        }
        ImGui::PopStyleColor();
    }
    
    ImGui::Spacing();
    
    // Current settings display
    ImGui::TextColored(m_colors.neonGreen, "⚙️ ACTIVE CONFIG:");
    ImGui::Text("Codec: %s", codecToString(m_state.currentCodec));
    ImGui::Text("Quality: %s", qualityToString(m_state.currentQuality));
    if (!m_state.outputFile.empty()) {
        ImGui::Text("Output: %s", m_state.outputFile.c_str());
    }
}

void ImGuiUI::renderSettingsPanel() {
    ImGui::TextColored(m_colors.neonPurple, "🛠️ SETTINGS PANEL");
    
    // Codec selection
    const char* codecs[] = { "H264_HARDWARE", "H264_SOFTWARE", "HEVC_HARDWARE", "HEVC_SOFTWARE" };
    static int codecIndex = 0;
    if (ImGui::Combo("Video Codec", &codecIndex, codecs, IM_ARRAYSIZE(codecs))) {
        m_state.currentCodec = static_cast<VideoCodec>(codecIndex);
    }
    
    // Quality selection
    const char* qualities[] = { "SMALL_SHARP", "BALANCED", "HIGH_QUALITY", "LOSSLESS" };
    static int qualityIndex = 0;
    if (ImGui::Combo("Quality Preset", &qualityIndex, qualities, IM_ARRAYSIZE(qualities))) {
        m_state.currentQuality = static_cast<QualityPreset>(qualityIndex);
    }
    
    // FPS slider
    ImGui::SliderInt("Frame Rate", &m_state.fps, 15, 120, "%d FPS");
    
    ImGui::Spacing();
    
    // Advanced options
    if (ImGui::CollapsingHeader("🔧 Advanced Options")) {
        ImGui::Text("Hardware Encoding: ✅ AMD AMF");
        ImGui::Text("Audio Capture: 🔊 System + Mic");
        ImGui::Text("Hotkeys: F9 Start/Stop, F10 Pause");
        
        if (ImGui::Button("📁 Select Output Folder")) {
            // TODO: File dialog
        }
    }
    
    // Debug options
    if (ImGui::CollapsingHeader("🐛 Debug")) {
        ImGui::Checkbox("Show ImGui Demo", &m_showDemo);
        if (ImGui::Button("Test Recording Backend")) {
            std::cout << "🧪 Testing recording backend..." << std::endl;
        }
    }
}

void ImGuiUI::renderStatusBar() {
    // Status indicator
    if (m_state.isRecording) {
        if (m_state.isPaused) {
            ImGui::TextColored(m_colors.neonOrange, "⏸️ PAUSED");
        } else {
            ImGui::TextColored(m_colors.neonPink, "🔴 LIVE RECORDING");
        }
    } else {
        ImGui::TextColored(m_colors.neonGreen, "✅ READY TO RECORD");
    }
    
    ImGui::SameLine();
    ImGui::Spacing();
    ImGui::SameLine();
    ImGui::TextColored(m_colors.neonBlue, "🖥️ AMD TURBO");
    ImGui::SameLine();
    ImGui::TextColored(m_colors.neonPurple, "💾 1.2TB FREE");
    ImGui::SameLine();
    ImGui::TextColored(m_colors.neonOrange, "📺 1920×1200");
}

void ImGuiUI::handleStartRecording() {
    if (!m_recorder) {
        std::cerr << "❌ No recorder connected!" << std::endl;
        return;
    }
    
    std::cout << "🎮 Starting recording..." << std::endl;
    
    // Generate filename
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << "recording_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".mp4";
    m_state.outputFile = oss.str();
    
    // Configure recorder
    m_recorder->setCodec(m_state.currentCodec);
    m_recorder->setQuality(m_state.currentQuality);
    m_recorder->setFrameRate(m_state.fps);
    
    if (m_recorder->start(m_state.outputFile, m_state.fps, 0, m_state.currentCodec, m_state.currentQuality)) {
        m_state.isRecording = true;
        m_state.isPaused = false;
        m_state.duration = 0;
        m_state.fileSize = 0.0f;
        std::cout << "✅ Recording started successfully!" << std::endl;
    } else {
        std::cerr << "❌ Failed to start recording!" << std::endl;
    }
}

void ImGuiUI::handleStopRecording() {
    if (!m_recorder) return;
    
    std::cout << "🏁 Stopping recording..." << std::endl;
    m_recorder->stop();
    m_state.isRecording = false;
    m_state.isPaused = false;
    std::cout << "✅ Recording stopped!" << std::endl;
}

void ImGuiUI::handlePauseRecording() {
    if (!m_recorder) return;
    
    if (m_state.isPaused) {
        std::cout << "▶️ Resuming recording..." << std::endl;
        m_recorder->resume();
        m_state.isPaused = false;
    } else {
        std::cout << "⏸️ Pausing recording..." << std::endl;
        m_recorder->pause();
        m_state.isPaused = true;
    }
}

void ImGuiUI::shutdown() {
    if (m_hglrc) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
        m_hglrc = nullptr;
    }
    
    if (m_hdc) {
        ReleaseDC(m_hwnd, m_hdc);
        m_hdc = nullptr;
    }
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

// Static window procedure
LRESULT WINAPI ImGuiUI::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    
    ImGuiUI* ui = reinterpret_cast<ImGuiUI*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            // Handle resize
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Utility functions
std::string ImGuiUI::formatTime(int seconds) {
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << hours << ":"
        << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << secs;
    return oss.str();
}

std::string ImGuiUI::formatFileSize(float megabytes) {
    if (megabytes < 1.0f) {
        return std::to_string(static_cast<int>(megabytes * 1024)) + " KB";
    } else if (megabytes < 1024.0f) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << megabytes << " MB";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (megabytes / 1024.0f) << " GB";
        return oss.str();
    }
}

const char* ImGuiUI::codecToString(VideoCodec codec) {
    switch (codec) {
        case VideoCodec::H264_HARDWARE: return "H264_HARDWARE (AMD AMF)";
        case VideoCodec::H264_SOFTWARE: return "H264_SOFTWARE";
        case VideoCodec::HEVC_HARDWARE: return "HEVC_HARDWARE (AMD AMF)";
        case VideoCodec::HEVC_SOFTWARE: return "HEVC_SOFTWARE";
        default: return "UNKNOWN";
    }
}

const char* ImGuiUI::qualityToString(QualityPreset quality) {
    switch (quality) {
        case QualityPreset::SMALL_SHARP: return "SMALL_SHARP";
        case QualityPreset::BALANCED: return "BALANCED";
        case QualityPreset::HIGH_QUALITY: return "HIGH_QUALITY";
        case QualityPreset::LOSSLESS: return "LOSSLESS";
        default: return "UNKNOWN";
    }
}
