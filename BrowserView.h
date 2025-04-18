// GameOverlay - BrowserView.h
// Manages browser rendering and DirectX 12 integration

#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <atomic> // For atomic flags
#include "RenderSystem.h"
#include "BrowserManager.h" // Include BrowserManager definition
#include "PerformanceOptimizer.h" // For performance state types

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

    // Process browser message loop (call this frequently)
    void Update();

    // Render browser content - This is now handled internally via Update and GPU copy
    // void Render(); // Removed - Update drives the process now

    // Called by BrowserManager when BrowserHandler::OnPaint fires
    void SignalTextureUpdateFromHandler(const void* buffer, int width, int height);

    // Check if a GPU copy is needed
    bool TextureNeedsGPUCopy() const { return m_textureNeedsGPUCopy; }
    void ClearTextureUpdateFlag() { m_textureNeedsGPUCopy = false; }
    const void* GetCpuBufferData() const { return m_cpuBufferData; } // Temp storage
    int GetCpuBufferWidth() const { return m_cpuBufferWidth; }
    int GetCpuBufferHeight() const { return m_cpuBufferHeight; }

    // Access to browser manager
    BrowserManager* GetBrowserManager() { return m_browserManager.get(); }

    // Texture access for ImGui / Rendering
    ID3D12Resource* GetTexture() const { return m_browserTexture.Get(); } // The target GPU texture
    ID3D12Resource* GetUploadTexture() const { return m_uploadTexture.Get(); } // The intermediate upload texture
    UINT GetSRVDescriptorIndex() const { return m_srvDescriptorIndex; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureGpuHandle() const; // Get GPU handle for ImGui::Image

    // Dimensions
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

    // Performance optimization
    void AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level);
    void SetRenderQuality(float quality); // Scales internal browser size
    float GetRenderQuality() const { return m_renderQuality; }
    void SetUpdateFrequency(int framesPerUpdate); // Throttles DoMessageLoopWork calls
    int GetUpdateFrequency() const { return m_framesPerUpdate; }
    void SuspendProcessing(bool suspend);
    bool IsProcessingSuspended() const { return m_processingIsSuspended; }


private:
    // Create texture resources (GPU texture and upload buffer)
    void CreateBrowserTextureResources(int width, int height);
    void ReleaseBrowserTextureResources();

    // DirectX 12 resources
    RenderSystem* m_renderSystem = nullptr;
    ComPtr<ID3D12Resource> m_browserTexture;        // Default heap texture (GPU-only) - Render target
    ComPtr<ID3D12Resource> m_uploadTexture;         // Upload heap texture (CPU write, GPU read for copy)
    UINT m_srvDescriptorIndex = UINT_MAX;           // SRV descriptor index for m_browserTexture

    // Browser resources
    std::unique_ptr<BrowserManager> m_browserManager;

    // Dimensions
    int m_width = 1024;  // Logical width of the view/texture
    int m_height = 768; // Logical height of the view/texture
    int m_browserInternalWidth = 1024;  // Actual width passed to CEF handler (scaled)
    int m_browserInternalHeight = 768; // Actual height passed to CEF handler (scaled)

    // Performance optimization
    float m_renderQuality = 1.0f; // Scales browser internal size
    std::atomic<int> m_framesPerUpdate = 1; // How often to call DoMessageLoopWork
    int m_frameCounter = 0;
    std::atomic<bool> m_processingIsSuspended = false;

    // Texture Update State
    std::atomic<bool> m_textureNeedsGPUCopy = false; // Flag indicating GPU copy is needed
    const void* m_cpuBufferData = nullptr;          // Temporary pointer to CPU buffer from OnPaint
    int m_cpuBufferWidth = 0;
    int m_cpuBufferHeight = 0;
    std::mutex m_bufferMutex; // Mutex for accessing CPU buffer details safely
};