// GameOverlay - RenderSystem.h
// Phase 1: Foundation Framework
// DirectX 11 rendering system for the overlay

#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <Windows.h>
#include "PerformanceOptimizer.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class ResourceManager;

class RenderSystem {
public:
    RenderSystem(HWND hwnd, int width, int height);
    ~RenderSystem();

    // Disable copy and move
    RenderSystem(const RenderSystem&) = delete;
    RenderSystem& operator=(const RenderSystem&) = delete;
    RenderSystem(RenderSystem&&) = delete;
    RenderSystem& operator=(RenderSystem&&) = delete;

    // Frame rendering methods
    void BeginFrame();
    void EndFrame();

    // Resize handling
    void Resize(int width, int height);

    // Performance optimization methods
    void SetRenderScale(float scale);
    float GetRenderScale() const { return m_renderScale; }
    void SetVSync(bool enabled);
    bool IsVSyncEnabled() const { return m_vsyncEnabled; }
    void AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level);

    // Resource management
    ResourceManager* GetResourceManager() const { return m_resourceManager.get(); }

    // Getters for ImGui integration
    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetDeviceContext() const { return m_deviceContext.Get(); }

private:
    void InitializeDirectX(HWND hwnd, int width, int height);
    void CreateRenderTarget();
    void CleanupRenderTarget();
    void CreateResources();
    void ReleaseResources();

    // DirectX objects
    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_deviceContext;
    ComPtr<IDXGISwapChain> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    // Performance optimization
    float m_renderScale = 1.0f;
    bool m_vsyncEnabled = true;
    std::unique_ptr<ResourceManager> m_resourceManager;

    // Window dimensions
    int m_width;
    int m_height;
    int m_scaledWidth;
    int m_scaledHeight;
    HWND m_hwnd;
};