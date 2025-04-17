// GameOverlay - BrowserView.cpp
// Phase 6: DirectX 12 Migration
// Manages browser rendering and DirectX 12 integration

#include "BrowserView.h"
#include "ResourceManager.h"
#include <stdexcept>
#include <algorithm>

BrowserView::BrowserView(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem),
    m_browserManager(std::make_unique<BrowserManager>()) {
}

BrowserView::~BrowserView() {
    Shutdown();
}

bool BrowserView::Initialize() {
    // Initialize browser manager
    if (!m_browserManager->Initialize(GetModuleHandle(NULL))) {
        return false;
    }

    // Create browser texture
    CreateBrowserTexture(m_width, m_height);

    // Create browser
    if (!m_browserManager->CreateBrowser("about:blank")) {
        return false;
    }

    return true;
}

void BrowserView::Shutdown() {
    // Release browser resources
    if (m_browserManager) {
        m_browserManager->Shutdown();
    }

    // Release texture resources
    ReleaseBrowserTexture();
}

void BrowserView::Navigate(const std::string& url) {
    if (m_browserManager) {
        m_browserManager->LoadURL(url);
    }
}

void BrowserView::Resize(int width, int height) {
    if (width <= 0 || height <= 0) return;

    // Apply render quality to dimensions
    int scaledWidth = static_cast<int>(width * m_renderQuality);
    int scaledHeight = static_cast<int>(height * m_renderQuality);

    // Ensure minimum dimensions
    scaledWidth = std::max(scaledWidth, 1);
    scaledHeight = std::max(scaledHeight, 1);

    // Update dimensions
    m_width = width;
    m_height = height;

    // Update browser handler size with scaled dimensions
    if (m_browserManager && m_browserManager->GetBrowserHandler()) {
        m_browserManager->GetBrowserHandler()->SetBrowserSize(scaledWidth, scaledHeight);
    }

    // Recreate browser texture with actual dimensions
    ReleaseBrowserTexture();
    CreateBrowserTexture(width, height);

    // Force an update
    m_needsUpdate = true;
}

void BrowserView::Render() {
    // Skip processing if suspended
    if (m_processingIsSuspended) {
        return;
    }

    // Apply frame limiting based on update frequency
    m_frameCounter++;
    if (m_frameCounter >= m_framesPerUpdate) {
        m_frameCounter = 0;
        m_needsUpdate = true;
    }

    // Process CEF message loop and render only when needed
    if (m_browserManager && m_needsUpdate) {
        m_browserManager->DoMessageLoopWork();

        // Set shared texture for rendering
        if (m_browserTexture && m_uploadTexture) {
            m_browserManager->OnPaint(m_uploadTexture.Get());
        }

        m_needsUpdate = false;
    }
}

void BrowserView::CreateBrowserTexture(int width, int height) {
    if (!m_renderSystem || width <= 0 || height <= 0) return;

    ID3D12Device* device = m_renderSystem->GetDevice();
    if (!device) return;

    // Get resource manager
    ResourceManager* resourceManager = m_renderSystem->GetResourceManager();
    if (!resourceManager) return;

    // Create texture in default heap (GPU-only)
    m_browserTexture = resourceManager->CreateTexture2D(
        width, height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATE_COPY_DEST
    );

    if (!m_browserTexture) {
        throw std::runtime_error("Failed to create browser texture");
    }

    // Create upload texture for CPU write
    size_t rowPitch = (width * 4 + 255) & ~255; // Align to 256 bytes
    size_t uploadBufferSize = rowPitch * height;

    m_uploadTexture = resourceManager->CreateUploadBuffer(uploadBufferSize);
    if (!m_uploadTexture) {
        throw std::runtime_error("Failed to create upload texture");
    }

    // Create shader resource view
    if (m_srvDescriptorIndex == UINT_MAX) {
        m_srvDescriptorIndex = resourceManager->AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = resourceManager->GetCpuDescriptorHandle(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srvDescriptorIndex);

    device->CreateShaderResourceView(m_browserTexture.Get(), &srvDesc, srvHandle);

    // Initial transition to shader resource state
    ID3D12GraphicsCommandList* commandList = m_renderSystem->GetCommandList();
    if (commandList) {
        resourceManager->TransitionResource(
            commandList,
            m_browserTexture.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
    }
}

void BrowserView::ReleaseBrowserTexture() {
    ResourceManager* resourceManager = m_renderSystem->GetResourceManager();
    if (resourceManager && m_srvDescriptorIndex != UINT_MAX) {
        resourceManager->FreeDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, m_srvDescriptorIndex);
        m_srvDescriptorIndex = UINT_MAX;
    }

    m_browserTexture.Reset();
    m_uploadTexture.Reset();
}

void BrowserView::UpdateBrowserTexture(const void* buffer, int width, int height) {
    if (!buffer || !m_uploadTexture || !m_browserTexture) return;

    ResourceManager* resourceManager = m_renderSystem->GetResourceManager();
    if (!resourceManager) return;

    // Map the upload buffer
    D3D12_RANGE readRange = { 0, 0 }; // We're not reading
    void* mappedData = nullptr;
    HRESULT hr = m_uploadTexture->Map(0, &readRange, &mappedData);
    if (FAILED(hr)) return;

    // Copy browser rendering to upload buffer
    size_t rowPitch = (width * 4 + 255) & ~255; // Align to 256 bytes
    uint8_t* dest = static_cast<uint8_t*>(mappedData);
    const uint8_t* src = static_cast<const uint8_t*>(buffer);

    for (int y = 0; y < height; y++) {
        memcpy(dest + y * rowPitch, src + y * width * 4, width * 4);
    }

    // Unmap the upload buffer
    D3D12_RANGE writtenRange = { 0, rowPitch * height };
    m_uploadTexture->Unmap(0, &writtenRange);

    // Get command list for copy operation
    ID3D12GraphicsCommandList* commandList = m_renderSystem->GetCommandList();
    if (!commandList) return;

    // Transition browser texture to copy dest state
    resourceManager->TransitionResource(
        commandList,
        m_browserTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST
    );

    // Copy from upload buffer to texture
    D3D12_TEXTURE_COPY_LOCATION srcLocation = {};
    srcLocation.pResource = m_uploadTexture.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint.Offset = 0;
    srcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    srcLocation.PlacedFootprint.Footprint.Width = width;
    srcLocation.PlacedFootprint.Footprint.Height = height;
    srcLocation.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.PlacedFootprint.Footprint.RowPitch = rowPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLocation = {};
    dstLocation.pResource = m_browserTexture.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = 0;

    commandList->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, nullptr);

    // Transition browser texture back to shader resource state
    resourceManager->TransitionResource(
        commandList,
        m_browserTexture.Get(),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
}

void BrowserView::AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level) {
    // Adjust browser parameters based on performance state
    switch (state) {
    case PerformanceState::Active:
        // Full quality when active
        SetRenderQuality(1.0f);
        SetUpdateFrequency(1);
        SuspendProcessing(false);
        break;

    case PerformanceState::Inactive:
        // Reduced quality when inactive
        SetRenderQuality(0.75f);
        SetUpdateFrequency(2);
        SuspendProcessing(false);
        break;

    case PerformanceState::Background:
        // Minimum quality when in background
        SetRenderQuality(0.5f);
        SetUpdateFrequency(5);
        SuspendProcessing(false);
        break;

    case PerformanceState::LowPower:
        // Suspend processing for power saving
        SetRenderQuality(0.25f);
        SetUpdateFrequency(10);
        SuspendProcessing(true);
        break;
    }

    // Further adjust based on resource usage level
    switch (level) {
    case ResourceUsageLevel::Minimum:
        // Minimum resources
        SetRenderQuality(0.25f);
        SetUpdateFrequency(10);
        if (state != PerformanceState::Active) {
            SuspendProcessing(true);
        }
        break;

    case ResourceUsageLevel::Low:
        // Low resources
        SetRenderQuality(0.5f);
        SetUpdateFrequency(5);
        break;

    case ResourceUsageLevel::Balanced:
        // Use state-determined settings
        break;

    case ResourceUsageLevel::High:
    case ResourceUsageLevel::Maximum:
        // Maximum quality if active
        if (state == PerformanceState::Active) {
            SetRenderQuality(1.0f);
            SetUpdateFrequency(1);
        }
        break;
    }
}

void BrowserView::SetRenderQuality(float quality) {
    // Clamp quality to reasonable values
    quality = std::max(0.25f, std::min(quality, 1.0f));

    if (m_renderQuality != quality) {
        m_renderQuality = quality;

        // Re-apply quality by resizing with current dimensions
        Resize(m_width, m_height);
    }
}

void BrowserView::SetUpdateFrequency(int framesPerUpdate) {
    // Clamp to reasonable values
    framesPerUpdate = std::max(1, std::min(framesPerUpdate, 30));

    if (m_framesPerUpdate != framesPerUpdate) {
        m_framesPerUpdate = framesPerUpdate;
        m_frameCounter = 0;
        m_needsUpdate = true;
    }
}

void BrowserView::SuspendProcessing(bool suspend) {
    if (m_processingIsSuspended != suspend) {
        m_processingIsSuspended = suspend;

        if (!suspend) {
            // Force an update when resuming
            m_needsUpdate = true;
        }
    }
}
