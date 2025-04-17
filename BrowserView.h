// GameOverlay - BrowserView.h
// Phase 6: DirectX 12 Migration
// Manages browser rendering and DirectX 12 integration

#pragma once

#include <d3d12.h>
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
    ID3D12Resource* GetTexture() const { return m_browserTexture.Get(); }
    UINT GetSRVDescriptorIndex() const { return m_srvDescriptorIndex; }

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
    void UpdateBrowserTexture(const void* buffer, int width, int height);

    // DirectX 12 resources
    RenderSystem* m_renderSystem = nullptr;
    ComPtr<ID3D12Resource> m_browserTexture;        // Default heap texture (GPU-only)
    ComPtr<ID3D12Resource> m_uploadTexture;         // Upload heap texture (for CPU write)
    UINT m_srvDescriptorIndex = UINT_MAX;           // SRV descriptor index in the heap

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