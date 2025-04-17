// GameOverlay - ImGuiSystem.cpp
// Phase 6: DirectX 12 Migration
// Dear ImGui integration for UI rendering with DirectX 12

#include "ImGuiSystem.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <stdexcept>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiSystem::ImGuiSystem(HWND hwnd, RenderSystem* renderSystem)
    : m_renderSystem(renderSystem), m_hwnd(hwnd) {
    InitializeImGui(hwnd, renderSystem);
}

ImGuiSystem::~ImGuiSystem() {
    ShutdownImGui();
}

void ImGuiSystem::InitializeImGui(HWND hwnd, RenderSystem* renderSystem) {
    // Create descriptor heap for ImGui
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(renderSystem->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_srvDescHeap)))) {
        throw std::runtime_error("Failed to create descriptor heap for ImGui");
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    m_imguiContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;

    // Setup Platform/Renderer backends
    if (!ImGui_ImplWin32_Init(hwnd)) {
        throw std::runtime_error("Failed to initialize ImGui Win32 backend");
    }

    if (!ImGui_ImplDX12_Init(
        renderSystem->GetDevice(),
        3, // Num frames in flight
        DXGI_FORMAT_R8G8B8A8_UNORM,
        m_srvDescHeap.Get(),
        m_srvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        m_srvDescHeap->GetGPUDescriptorHandleForHeapStart())) {
        throw std::runtime_error("Failed to initialize ImGui DirectX 12 backend");
    }

    // Load fonts
    io.Fonts->AddFontDefault();

    // Add additional fonts for UI system
    ImFontConfig config;
    config.MergeMode = false;

    // Larger font for headers (size 18)
    static const ImWchar ranges[] = { 0x0020, 0xFFFF, 0 };
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &config, ranges);

    // Medium font for subheadings (size 16)
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &config, ranges);

    // Icon font for UI elements
    static const ImWchar icons_ranges[] = { 0xF000, 0xF3FF, 0 };
    ImFontConfig icons_config;
    icons_config.MergeMode = true;
    icons_config.PixelSnapH = true;
    icons_config.GlyphMinAdvanceX = 13.0f;
    icons_config.GlyphOffset = ImVec2(0, 0);

    // Note: In a real implementation, you'd include a proper icon font like Font Awesome
    // For demo purposes, we're just using the default font as a placeholder
}

void ImGuiSystem::ShutdownImGui() {
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(m_imguiContext);
    m_imguiContext = nullptr;
}

void ImGuiSystem::BeginFrame() {
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiSystem::EndFrame() {
    // Render Dear ImGui
    ImGui::Render();

    // Set descriptor heaps
    ID3D12DescriptorHeap* heaps[] = { m_srvDescHeap.Get() };
    m_renderSystem->GetCommandList()->SetDescriptorHeaps(_countof(heaps), heaps);

    // Render ImGui draw data
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_renderSystem->GetCommandList());

    // Update and Render additional Platform Windows
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, (void*)m_renderSystem->GetCommandList());
    }
}

void ImGuiSystem::RenderDemoWindow() {
    // Show ImGui demo window for testing
    ImGui::ShowDemoWindow(&m_showDemoWindow);

    // Simple overlay window for FPS display
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("GameOverlay", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::Text("GameOverlay - Phase 6: DirectX 12 Migration");
        ImGui::Separator();
        ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    }
    ImGui::End();
}

LRESULT ImGuiSystem::ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Forward to ImGui message handler
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return true;

    return 0;
}