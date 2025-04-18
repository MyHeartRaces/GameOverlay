// GameOverlay - BrowserView.cpp
// Manages browser rendering and DirectX 12 integration

#include "BrowserView.h"
#include "ResourceManager.h" // Include ResourceManager
#include <stdexcept>
#include <algorithm>
#include <vector> // For intermediate buffer copy

BrowserView::BrowserView(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem),
    // Pass 'this' to BrowserManager for signalling
    m_browserManager(std::make_unique<BrowserManager>(this))
{
    if (!m_renderSystem) {
        throw std::invalid_argument("RenderSystem cannot be null");
    }
}

BrowserView::~BrowserView() {
    Shutdown();
}

bool BrowserView::Initialize() {
    // Initialize browser manager
    if (!m_browserManager->Initialize(GetModuleHandle(NULL))) {
        // Initialization might fail if it's a CEF subprocess, which is expected.
        // Or it could be a real error. BrowserManager returns false for subprocess.
        // We might need a better way to distinguish these cases if necessary.
        // OutputDebugStringA("BrowserManager Initialize failed.\n");
        // For now, assume failure means error if not a subprocess (manager handles that check)
        if (!m_browserManager->m_isSubprocess) { // Accessing member directly - consider a getter
            OutputDebugStringA("Error: BrowserManager failed to initialize CEF.\n");
            return false;
        }
        // If it's a subprocess, Initialize returns false but it's not an error state for the main app.
        // The subprocess will exit via CefExecuteProcess. The main app continues.
        // So we might return true here conceptually, but the main app needs to handle
        // the case where the manager isn't fully usable.
        // Let's refine: Initialize should only return false on *actual* error in the main process.
    }

    // Create browser texture resources (GPU texture + upload buffer)
    CreateBrowserTextureResources(m_width, m_height);

    // Create the actual browser instance
    // Make sure manager is actually initialized (not a subprocess)
    if (m_browserManager->m_initialized) { // Check initialization state
        if (!m_browserManager->CreateBrowser("about:blank")) {
            OutputDebugStringA("Error: Failed to create CEF browser instance.\n");
            ReleaseBrowserTextureResources(); // Clean up textures if browser fails
            return false;
        }
        // Set initial size for the browser handler via the manager
        m_browserInternalWidth = static_cast<int>(m_width * m_renderQuality);
        m_browserInternalHeight = static_cast<int>(m_height * m_renderQuality);
        if (m_browserManager->GetBrowserHandler()) {
            m_browserManager->GetBrowserHandler()->SetBrowserSize(m_browserInternalWidth, m_browserInternalHeight);
        }
        // Tell the host it was resized after creation
        if (m_browserManager->GetBrowser() && m_browserManager->GetBrowser()->GetHost()) {
            m_browserManager->GetBrowser()->GetHost()->WasResized();
        }
    }
    else {
        // This case implies Initialize returned false due to being a subprocess.
        // The main application instance doesn't need a browser in this case.
        // It might need adjustment depending on how subprocess handling is intended.
        // Let's assume Initialize throws on real error, and returns false only for subprocess.
    }

    return true;
}

void BrowserView::Shutdown() {
    // Release browser resources first
    if (m_browserManager) {
        m_browserManager->Shutdown();
    }
    m_browserManager.reset(); // Release manager itself

    // Release texture resources
    ReleaseBrowserTextureResources();

    // Nullify render system pointer (it's not owned)
    m_renderSystem = nullptr;
}

void BrowserView::Navigate(const std::string& url) {
    if (m_browserManager && m_browserManager->m_initialized) {
        m_browserManager->LoadURL(url);
    }
}

void BrowserView::Resize(int width, int height) {
    if (!m_renderSystem || width <= 0 || height <= 0) return;

    bool needsResize = (m_width != width || m_height != height);
    bool internalSizeChanged = false;

    m_width = width;
    m_height = height;

    // Apply render quality to get internal browser dimensions
    int newInternalWidth = std::max(1, static_cast<int>(width * m_renderQuality));
    int newInternalHeight = std::max(1, static_cast<int>(height * m_renderQuality));

    if (m_browserInternalWidth != newInternalWidth || m_browserInternalHeight != newInternalHeight) {
        m_browserInternalWidth = newInternalWidth;
        m_browserInternalHeight = newInternalHeight;
        internalSizeChanged = true;
    }

    // Update browser handler size if it changed
    if (internalSizeChanged && m_browserManager && m_browserManager->GetBrowserHandler()) {
        m_browserManager->GetBrowserHandler()->SetBrowserSize(m_browserInternalWidth, m_browserInternalHeight);
        // Notify the browser host about the resize
        if (m_browserManager->GetBrowser() && m_browserManager->GetBrowser()->GetHost()) {
            m_browserManager->GetBrowser()->GetHost()->WasResized();
        }
    }

    // Recreate texture resources only if logical dimensions changed
    if (needsResize) {
        // Wait for GPU before releasing resources tied to the swap chain/command lists
        m_renderSystem->WaitForGpu();
        ReleaseBrowserTextureResources();
        CreateBrowserTextureResources(m_width, m_height);
    }

    // Force an update check in the message loop
    m_textureNeedsGPUCopy = true; // Assume resize requires redraw
}

void BrowserView::Update() {
    // Skip processing if suspended
    if (m_processingIsSuspended) {
        return;
    }

    // Process message loop only according to frequency
    m_frameCounter++;
    if (m_frameCounter >= m_framesPerUpdate) {
        m_frameCounter = 0;
        if (m_browserManager && m_browserManager->m_initialized) {
            m_browserManager->DoMessageLoopWork();
        }
    }
    // Note: OnPaint is triggered by DoMessageLoopWork and calls SignalTextureUpdateFromHandler
}

// Called by BrowserManager when BrowserHandler::OnPaint fires
void BrowserView::SignalTextureUpdateFromHandler(const void* buffer, int width, int height) {
    // This is called from CEF's thread, so use a mutex if accessing shared members that aren't atomic
    std::lock_guard<std::mutex> lock(m_bufferMutex);
    m_cpuBufferData = buffer; // Store pointer temporarily
    m_cpuBufferWidth = width;
    m_cpuBufferHeight = height;
    m_textureNeedsGPUCopy = true; // Set the flag indicating GPU copy is required
}

D3D12_GPU_DESCRIPTOR_HANDLE BrowserView::GetTextureGpuHandle() const {
    if (m_renderSystem && m_renderSystem->GetResourceManager() && m_srvDescriptorIndex != UINT_MAX) {
        // Ensure the descriptor index is valid before getting the handle
        return m_renderSystem->GetResourceManager()->GetGpuDescriptorHandle(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srvDescriptorIndex
        );
    }
    return {}; // Return a null handle if invalid
}


void BrowserView::CreateBrowserTextureResources(int width, int height) {
    if (!m_renderSystem || width <= 0 || height <= 0) return;

    ID3D12Device* device = m_renderSystem->GetDevice();
    ResourceManager* resourceManager = m_renderSystem->GetResourceManager();
    if (!device || !resourceManager) return;

    // Ensure previous resources are released (important if called during resize)
    ReleaseBrowserTextureResources();

    // 1. Create the target texture in the default heap (GPU optimal)
    m_browserTexture = resourceManager->CreateTexture2D(
        width, height,
        DXGI_FORMAT_B8G8R8A8_UNORM, // Format CEF typically provides (BGRA)
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE // Initial state for rendering
    );

    if (!m_browserTexture) {
        throw std::runtime_error("Failed to create browser target texture (GPU)");
    }
    m_browserTexture->SetName(L"Browser Target Texture"); // Debug name

    // 2. Create the upload heap texture for CPU writes
    // Calculate required size based on pitch alignment
    // BGRA is 4 bytes per pixel
    size_t rowPitch = (static_cast<size_t>(width) * 4 + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1) & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    size_t uploadBufferSize = rowPitch * height;

    m_uploadTexture = resourceManager->CreateUploadBuffer(uploadBufferSize);
    if (!m_uploadTexture) {
        throw std::runtime_error("Failed to create browser upload texture (CPU)");
    }
    m_uploadTexture->SetName(L"Browser Upload Texture"); // Debug name

    // 3. Create Shader Resource View (SRV) for the target texture
    m_srvDescriptorIndex = resourceManager->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (m_srvDescriptorIndex == UINT_MAX) {
        throw std::runtime_error("Failed to allocate descriptor for browser texture SRV");
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // Match texture format
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = resourceManager->GetCpuDescriptorHandle(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srvDescriptorIndex);

    if (srvHandle.ptr == 0) {
        throw std::runtime_error("Failed to get CPU descriptor handle for browser SRV");
    }

    device->CreateShaderResourceView(m_browserTexture.Get(), &srvDesc, srvHandle);

    // No initial transition needed here if CreateTexture2D creates it in PIXEL_SHADER_RESOURCE state
}

void BrowserView::ReleaseBrowserTextureResources() {
    ResourceManager* resourceManager = m_renderSystem ? m_renderSystem->GetResourceManager() : nullptr;

    // Free the descriptor if allocated
    if (resourceManager && m_srvDescriptorIndex != UINT_MAX) {
        resourceManager->FreeDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srvDescriptorIndex);
        m_srvDescriptorIndex = UINT_MAX;
    }

    // Release the texture resources (ComPtr handles this)
    m_browserTexture.Reset();
    m_uploadTexture.Reset();

    // Reset update state
    m_textureNeedsGPUCopy = false;
    m_cpuBufferData = nullptr;
    m_cpuBufferWidth = 0;
    m_cpuBufferHeight = 0;
}

// --- Performance Optimization Methods ---

void BrowserView::AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level) {
    // Adjust browser parameters based on performance state
    float targetQuality = 1.0f;
    int targetUpdateFreq = 1;
    bool targetSuspend = false;

    switch (state) {
    case PerformanceState::Active:
        targetQuality = 1.0f; targetUpdateFreq = 1; targetSuspend = false;
        break;
    case PerformanceState::Inactive:
        targetQuality = 0.75f; targetUpdateFreq = 3; targetSuspend = false; // Less frequent updates
        break;
    case PerformanceState::Background:
        targetQuality = 0.5f; targetUpdateFreq = 10; targetSuspend = true; // Suspend if configured
        break;
    case PerformanceState::LowPower:
        targetQuality = 0.25f; targetUpdateFreq = 15; targetSuspend = true;
        break;
    }

    // Further adjust based on resource usage level
    switch (level) {
    case ResourceUsageLevel::Minimum:
        targetQuality = std::min(targetQuality, 0.25f);
        targetUpdateFreq = std::max(targetUpdateFreq, 15);
        if (state != PerformanceState::Active) targetSuspend = true;
        break;
    case ResourceUsageLevel::Low:
        targetQuality = std::min(targetQuality, 0.5f);
        targetUpdateFreq = std::max(targetUpdateFreq, 10);
        if (state == PerformanceState::Background || state == PerformanceState::LowPower) targetSuspend = true;
        break;
    case ResourceUsageLevel::Balanced:
        // Use state-determined settings
        break;
    case ResourceUsageLevel::High:
    case ResourceUsageLevel::Maximum:
        // Maximum quality if active, less aggressive otherwise
        if (state == PerformanceState::Active) {
            targetQuality = 1.0f;
            targetUpdateFreq = 1;
            targetSuspend = false;
        }
        else {
            // Keep state defaults for inactive/background
        }
        break;
    }

    // Apply the calculated settings
    SetRenderQuality(targetQuality);
    SetUpdateFrequency(targetUpdateFreq);
    SuspendProcessing(targetSuspend);

    // Optional: Tell CEF about visibility/focus changes for its own optimizations
    if (m_browserManager && m_browserManager->GetBrowser() && m_browserManager->GetBrowser()->GetHost()) {
        bool isVisible = (state != PerformanceState::Background && state != PerformanceState::LowPower);
        bool hasFocus = (state == PerformanceState::Active); // Assuming active means focused
        m_browserManager->GetBrowser()->GetHost()->WasHidden(!isVisible);
        m_browserManager->GetBrowser()->GetHost()->SetFocus(hasFocus);

        // Optional: Send occlusion notification
        // std::vector<CefRect> occlusion_rects;
        // if (!isVisible) occlusion_rects.push_back(CefRect(0,0,m_width,m_height));
        // m_browserManager->GetBrowser()->GetHost()->SendExternalBeginFrame();
        // m_browserManager->GetBrowser()->GetHost()->SendTouchEvent() ...
    }
}

void BrowserView::SetRenderQuality(float quality) {
    quality = std::max(0.1f, std::min(quality, 1.0f)); // Allow lower minimum quality

    if (m_renderQuality != quality) {
        m_renderQuality = quality;
        // Trigger a resize to apply the new internal browser dimensions
        Resize(m_width, m_height);
    }
}

void BrowserView::SetUpdateFrequency(int framesPerUpdate) {
    framesPerUpdate = std::max(1, std::min(framesPerUpdate, 60)); // Clamp to reasonable range

    if (m_framesPerUpdate != framesPerUpdate) {
        m_framesPerUpdate = framesPerUpdate;
        m_frameCounter = 0; // Reset counter
    }
}

void BrowserView::SuspendProcessing(bool suspend) {
    if (m_processingIsSuspended != suspend) {
        m_processingIsSuspended = suspend;
        if (!suspend) {
            // Force an update check when resuming
            m_textureNeedsGPUCopy = true; // Mark for redraw
        }
        // Optional: Tell CEF host directly about suspension state if API exists
    }
}