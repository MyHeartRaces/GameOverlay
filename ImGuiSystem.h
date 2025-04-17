// GameOverlay - ImGuiSystem.h
// Phase 6: DirectX 12 Migration
// Dear ImGui integration for UI rendering with DirectX 12

#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include "RenderSystem.h"

// Forward declarations
struct ImGuiContext;

class ImGuiSystem {
public:
    ImGuiSystem(HWND hwnd, RenderSystem* renderSystem);
    ~ImGuiSystem();

    // Disable copy and move
    ImGuiSystem(const ImGuiSystem&) = delete;
    ImGuiSystem& operator=(const ImGuiSystem&) = delete;
    ImGuiSystem(ImGuiSystem&&) = delete;
    ImGuiSystem& operator=(ImGuiSystem&&) = delete;

    // Frame methods
    void BeginFrame();
    void EndFrame();

    // Demo window for testing
    void RenderDemoWindow();

    // Process Windows messages for ImGui
    static LRESULT ProcessMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void InitializeImGui(HWND hwnd, RenderSystem* renderSystem);
    void ShutdownImGui();

    ImGuiContext* m_imguiContext = nullptr;
    RenderSystem* m_renderSystem = nullptr;
    HWND m_hwnd = nullptr;
    bool m_showDemoWindow = true;

    // DirectX 12 specific resources
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvDescHeap;
    UINT m_fontDescriptorIndex = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_fontTextureResource;
};