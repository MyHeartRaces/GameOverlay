// GameOverlay - BrowserView.h
// Phase 2: CEF Integration
// Manages browser rendering and DirectX integration

#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include "RenderSystem.h"
#include "BrowserManager.h"

using Microsoft::WRL::ComPtr;

class BrowserView {
public:
    BrowserView(RenderSystem* renderSystem);
    ~BrowserView();

    // Disable copy and move
    BrowserView(const BrowserView&) = delete;
    BrowserView& operator=(const BrowserView&) = delete;
    BrowserView(BrowserView&&) = delete;
    BrowserView& operator=(BrowserView&&) = delete;

    // Browser control
    bool Initialize();
    void Shutdown();
    void Navigate(const std::string& url);
    void Resize(int width, int height);

    // Render browser content
    void Render();

    // Access to browser manager
    BrowserManager* GetBrowserManager() { return m_browserManager.get(); }

    // Texture access for ImGui
    ID3D11ShaderResourceView* GetShaderResourceView() const { return m_shaderResourceView.Get(); }

    // Performance optimization
    void AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level);
    void SetRenderQuality(float quality);
    float GetRenderQuality() const { return m_renderQuality; }
    void SetUpdateFrequency(int framesPerUpdate);
    int GetUpdateFrequency() const { return m_framesPerUpdate; }
    void SuspendProcessing(bool suspend);
    bool IsProcessingSuspended() const { return m_processingIsSuspended; }

private:
    // Create texture resources
    void CreateBrowserTexture(int width, int height);
    void ReleaseBrowserTexture();

    // DirectX resources
    RenderSystem* m_renderSystem = nullptr;
    ComPtr<ID3D11Texture2D> m_browserTexture;
    ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;

    // Browser resources
    std::unique_ptr<BrowserManager> m_browserManager;

    // Dimensions
    int m_width = 1024;
    int m_height = 768;

    // Performance optimization
    float m_renderQuality = 1.0f;
    int m_framesPerUpdate = 1;
    int m_frameCounter = 0;
    bool m_processingIsSuspended = false;
    bool m_needsUpdate = true;
};