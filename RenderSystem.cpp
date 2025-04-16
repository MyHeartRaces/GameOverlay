void RenderSystem::CreateResources() {
    // This method would create any common resources needed for rendering
    // such as shaders, constant buffers, etc.
}

void RenderSystem::ReleaseResources() {
    // Release any created resources
    if (m_resourceManager) {
        m_resourceManager->ClearCache();
    }
}void RenderSystem::SetRenderScale(float scale) {
    // Clamp scale to reasonable values
    scale = std::max(0.25f, std::min(scale, 1.0f));

    if (m_renderScale != scale) {
        m_renderScale = scale;

        // Recalculate scaled dimensions
        m_scaledWidth = static_cast<int>(m_width * m_renderScale);
        m_scaledHeight = static_cast<int>(m_height * m_renderScale);

        // Ensure minimum size
        m_scaledWidth = std::max(m_scaledWidth, 1);
        m_scaledHeight = std::max(m_scaledHeight, 1);

        // Update viewport
        D3D11_VIEWPORT vp;
        vp.Width = static_cast<float>(m_scaledWidth);
        vp.Height = static_cast<float>(m_scaledHeight);
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        m_deviceContext->RSSetViewports(1, &vp);
    }
}

void RenderSystem::SetVSync(bool enabled) {
    m_vsyncEnabled = enabled;
}

void RenderSystem::AdaptToPerformanceState(PerformanceState state, ResourceUsageLevel level) {
    // Adjust rendering parameters based on performance state
    switch (state) {
    case PerformanceState::Active:
        // Full quality when active
        SetRenderScale(1.0f);
        SetVSync(true);
        break;

    case PerformanceState::Inactive:
        // Reduced quality when inactive
        SetRenderScale(0.75f);
        SetVSync(true);
        break;

    case PerformanceState::Background:
        // Minimum quality when in background
        SetRenderScale(0.5f);
        SetVSync(true);
        break;

    case PerformanceState::LowPower:
        // Absolute minimum for power saving
        SetRenderScale(0.25f);
        SetVSync(true);
        break;
    }

    // Further adjust based on resource usage level
    switch (level) {
    case ResourceUsageLevel::Minimum:
        // Override scale for minimum resource usage
        SetRenderScale(0.25f);
        break;

    case ResourceUsageLevel::Low:
        // Reduce scale further
        SetRenderScale(m_renderScale * 0.75f);
        break;

    case ResourceUsageLevel::Balanced:
        // Use state-determined scale
        break;

    case ResourceUsageLevel::High:
    case ResourceUsageLevel::Maximum:
        // Increase quality if state allows
        if (state == PerformanceState::Active) {
            SetRenderScale(1.0f);
            SetVSync(false); // Disable vsync for maximum performance
        }
        break;
    }

    // Clean up resources if in background state
    if (state == PerformanceState::Background || state == PerformanceState::LowPower) {
        if (m_resourceManager) {
            m_resourceManager->ReleaseUnusedResources(std::chrono::seconds(10));
        }
    }
}// GameOverlay - RenderSystem.cpp
// Phase 1: Foundation Framework
// DirectX 11 rendering system for the overlay

#include "RenderSystem.h"
#include "ResourceManager.h"
#include <stdexcept>
#include <string>
#include <algorithm>

RenderSystem::RenderSystem(HWND hwnd, int width, int height)
    : m_width(width), m_height(height), m_scaledWidth(width), m_scaledHeight(height), m_hwnd(hwnd) {
    InitializeDirectX(hwnd, width, height);
    m_resourceManager = std::make_unique<ResourceManager>(this);
    CreateResources();
}

RenderSystem::~RenderSystem() {
    // DirectX resources are automatically released by ComPtr
    ReleaseResources();
    CleanupRenderTarget();
}

void RenderSystem::InitializeDirectX(HWND hwnd, int width, int height) {
    // Create device and swap chain
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        1,
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_deviceContext
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create D3D11 device and swap chain");
    }

    // Create render target view
    CreateRenderTarget();

    // Setup viewport
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_deviceContext->RSSetViewports(1, &vp);
}

void RenderSystem::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to get swap chain buffer");
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTargetView);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create render target view");
    }
}

void RenderSystem::CleanupRenderTarget() {
    if (m_renderTargetView) {
        m_renderTargetView.Reset();
    }
}

void RenderSystem::BeginFrame() {
    // Clear the render target
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Fully transparent black
    m_deviceContext->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
    m_deviceContext->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    // Set viewport based on render scale
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(m_scaledWidth);
    vp.Height = static_cast<float>(m_scaledHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_deviceContext->RSSetViewports(1, &vp);
}

void RenderSystem::EndFrame() {
    // Present the frame
    // Using vsync (1) to prevent excessive GPU usage when enabled
    m_swapChain->Present(m_vsyncEnabled ? 1 : 0, 0);
}

void RenderSystem::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || !m_device) return;

    m_width = width;
    m_height = height;

    // Calculate scaled dimensions
    m_scaledWidth = static_cast<int>(m_width * m_renderScale);
    m_scaledHeight = static_cast<int>(m_height * m_renderScale);

    // Ensure minimum size
    m_scaledWidth = std::max(m_scaledWidth, 1);
    m_scaledHeight = std::max(m_scaledHeight, 1);

    // Release old views and buffers
    CleanupRenderTarget();

    // Resize swap chain buffers
    HRESULT hr = m_swapChain->ResizeBuffers(
        0,
        m_width,  // Always use native size for swap chain
        m_height,
        DXGI_FORMAT_UNKNOWN,
        0
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to resize swap chain buffers");
    }

    // Recreate render target
    CreateRenderTarget();

    // Update viewport
    D3D11_VIEWPORT vp;
    vp.Width = static_cast<float>(m_scaledWidth);
    vp.Height = static_cast<float>(m_scaledHeight);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_deviceContext->RSSetViewports(1, &vp);
}