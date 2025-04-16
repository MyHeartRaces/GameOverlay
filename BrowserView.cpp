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
}// GameOverlay - BrowserView.cpp
// Phase 2: CEF Integration
// Manages browser rendering and DirectX integration

#include "BrowserView.h"
#include <stdexcept>

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
        if (m_browserTexture) {
            m_browserManager->OnPaint(m_browserTexture.Get());
        }

        m_needsUpdate = false;
    }
}

void BrowserView::CreateBrowserTexture(int width, int height) {
    if (!m_renderSystem || width <= 0 || height <= 0) return;

    ID3D11Device* device = m_renderSystem->GetDevice();
    if (!device) return;

    // Check if ResourceManager is available
    ResourceManager* resourceManager = m_renderSystem->GetResourceManager();
    if (resourceManager) {
        // Use ResourceManager to create texture
        m_browserTexture = resourceManager->CreateTexture2D(
            width, height,
            DXGI_FORMAT_B8G8R8A8_UNORM,
            D3D11_BIND_SHADER_RESOURCE,
            D3D11_USAGE_DYNAMIC
        );

        // Create shader resource view
        if (m_browserTexture) {
            m_shaderResourceView = resourceManager->CreateShaderResourceView(m_browserTexture.Get());
        }
    }
    else {
        // Create texture directly (fallback)
        D3D11_TEXTURE2D_DESC textureDesc = {};
        textureDesc.Width = width;
        textureDesc.Height = height;
        textureDesc.MipLevels = 1;
        textureDesc.ArraySize = 1;
        textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Usage = D3D11_USAGE_DYNAMIC;
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        textureDesc.MiscFlags = 0;

        HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, &m_browserTexture);
        if (FAILED(hr) || !m_browserTexture) {
            throw std::runtime_error("Failed to create browser texture");
        }

        // Create shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        hr = device->CreateShaderResourceView(m_browserTexture.Get(), &srvDesc, &m_shaderResourceView);
        if (FAILED(hr) || !m_shaderResourceView) {
            throw std::runtime_error("Failed to create browser shader resource view");
        }
    }
}

void BrowserView::ReleaseBrowserTexture() {
    m_shaderResourceView.Reset();
    m_browserTexture.Reset();
}
