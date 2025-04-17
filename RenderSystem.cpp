// GameOverlay - RenderSystem.cpp
// Phase 6: DirectX 12 Migration
// DirectX 12 rendering system for the overlay

#include "RenderSystem.h"
#include "ResourceManager.h"
#include <stdexcept>
#include <string>
#include <algorithm>
#include <d3dcompiler.h>

// Implementation of FrameContext
FrameContext::FrameContext(ID3D12Device* device) {
    HRESULT hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create command allocator");
    }
}

void FrameContext::Reset() {
    HRESULT hr = commandAllocator->Reset();
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to reset command allocator");
    }
}

// Implementation of DescriptorHeapManager
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetRtvHandle(UINT index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * rtvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetDsvHandle(UINT index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * dsvDescriptorSize;
    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetCbvSrvUavCpuHandle(UINT index) const {
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += index * cbvSrvUavDescriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::GetCbvSrvUavGpuHandle(UINT index) const {
    D3D12_GPU_DESCRIPTOR_HANDLE handle = cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += index * cbvSrvUavDescriptorSize;
    return handle;
}

// RenderSystem implementation
RenderSystem::RenderSystem(HWND hwnd, int width, int height)
    : m_width(width), m_height(height), m_scaledWidth(width), m_scaledHeight(height), m_hwnd(hwnd) {

    InitializeDirectX12(hwnd, width, height);
    m_descriptorManager = std::make_unique<DescriptorHeapManager>();
    m_resourceManager = std::make_unique<ResourceManager>(this);
}

RenderSystem::~RenderSystem() {
    // Wait for GPU to finish before destroying resources
    WaitForGpu();

    // Close fence event handle
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // Release resources
    ReleaseResources();
}

void RenderSystem::InitializeDirectX12(HWND hwnd, int width, int height) {
    // Enable the debug layer in debug builds
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    // Check for tearing support
    CheckTearingSupport();

    // Create DXGI factory
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create DXGI factory");
    }

    // Try to create hardware device
    ComPtr<IDXGIAdapter1> adapter;

    // Try to find a high-performance adapter
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Check if adapter supports Direct3D 12
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
            break;
        }
    }

    // Fall back to WARP adapter if no hardware adapter found
    if (adapter == nullptr) {
        factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        m_useWarpAdapter = true;
    }

    // Create Direct3D 12 device
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create D3D12 device");
    }

    // Create command queue, allocators, and list
    CreateCommandObjects();

    // Create swap chain
    CreateSwapChain(hwnd, width, height);

    // Create descriptor heaps
    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 3; // Triple buffering
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_descriptorManager->rtvHeap));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create RTV descriptor heap");
    }

    // Create CBV/SRV/UAV heap for ImGui and other resources
    D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavHeapDesc = {};
    cbvSrvUavHeapDesc.NumDescriptors = 1000; // Plenty of descriptors
    cbvSrvUavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvSrvUavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    hr = m_device->CreateDescriptorHeap(&cbvSrvUavHeapDesc, IID_PPV_ARGS(&m_descriptorManager->cbvSrvUavHeap));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create CBV/SRV/UAV descriptor heap");
    }

    // Set descriptor sizes
    m_descriptorManager->rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_descriptorManager->dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_descriptorManager->cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_descriptorManager->samplerDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // Create render targets
    CreateRenderTargets();

    // Create synchronization objects
    CreateSyncObjects();

    // Initialize per-frame contexts
    for (int i = 0; i < 3; i++) {
        m_frameContexts[i] = std::make_unique<FrameContext>(m_device.Get());
    }
}

void RenderSystem::CreateCommandObjects() {
    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create command queue");
    }

    // Create command allocators
    for (int i = 0; i < 3; i++) {
        hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]));
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to create command allocator");
        }
    }

    // Create command list
    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create command list");
    }

    // Close the command list as it's initially opened
    m_commandList->Close();
}

void RenderSystem::CreateSwapChain(HWND hwnd, int width, int height) {
    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create DXGI factory");
    }

    // Create swap chain description
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 3; // Triple buffering
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create swap chain");
    }

    // Convert to IDXGISwapChain3
    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to query IDXGISwapChain3 interface");
    }

    // Get initial frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void RenderSystem::CreateRenderTargets() {
    for (UINT i = 0; i < 3; i++) {
        // Get buffer from swap chain
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to get swap chain buffer");
        }

        // Create RTV
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_descriptorManager->GetRtvHandle(i);
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
    }
}

void RenderSystem::CreateSyncObjects() {
    // Create fence
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create fence");
    }

    // Create fence event
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr) {
        throw std::runtime_error("Failed to create fence event");
    }
}

void RenderSystem::CheckTearingSupport() {
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    if (SUCCEEDED(hr)) {
        ComPtr<IDXGIFactory5> factory5;
        hr = factory.As(&factory5);

        if (SUCCEEDED(hr)) {
            BOOL allowTearing = FALSE;
            hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

            if (SUCCEEDED(hr)) {
                m_tearingSupported = (allowTearing == TRUE);
            }
        }
    }
}

void RenderSystem::BeginFrame() {
    // Wait for the previous frame to finish
    auto& frameContext = m_frameContexts[m_frameIndex];
    frameContext->Reset();

    // Reset command list and allocator
    HRESULT hr = m_commandAllocators[m_frameIndex]->Reset();
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to reset command allocator");
    }

    hr = m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), nullptr);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to reset command list");
    }

    // Transition render target to render target state
    ID3D12Resource* currentRenderTarget = GetCurrentRenderTarget();
    TransitionResource(currentRenderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Set render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = GetCurrentRenderTargetView();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Clear render target
    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Fully transparent black
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Set viewport and scissor rect based on render scale
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_scaledWidth);
    viewport.Height = static_cast<float>(m_scaledHeight);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = m_scaledWidth;
    scissorRect.bottom = m_scaledHeight;

    m_commandList->RSSetViewports(1, &viewport);
    m_commandList->RSSetScissorRects(1, &scissorRect);
}

void RenderSystem::EndFrame() {
    // Transition render target to present state
    ID3D12Resource* currentRenderTarget = GetCurrentRenderTarget();
    TransitionResource(currentRenderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    // Close command list
    HRESULT hr = m_commandList->Close();
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to close command list");
    }

    // Execute command list
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame
    UINT syncInterval = m_vsyncEnabled ? 1 : 0;
    UINT presentFlags = (m_tearingSupported && !m_vsyncEnabled) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    hr = m_swapChain->Present(syncInterval, presentFlags);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to present swap chain");
    }

    // Signal and advance frame
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    hr = m_commandQueue->Signal(m_fence.Get(), currentFenceValue);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to signal fence");
    }

    // Move to next frame
    MoveToNextFrame();
}

void RenderSystem::MoveToNextFrame() {
    // Update the frame index
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is
    if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex]) {
        HRESULT hr = m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to set fence event");
        }

        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Set the fence value for the next frame
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

ID3D12Resource* RenderSystem::GetCurrentRenderTarget() const {
    return m_renderTargets[m_frameIndex].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderSystem::GetCurrentRenderTargetView() const {
    return m_descriptorManager->GetRtvHandle(m_frameIndex);
}

UINT RenderSystem::GetCurrentBackBufferIndex() const {
    return m_frameIndex;
}

void RenderSystem::WaitForGpu() {
    // Schedule a signal command
    HRESULT hr = m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to signal fence");
    }

    // Wait until the GPU has completed the command
    hr = m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to set fence event");
    }

    WaitForSingleObject(m_fenceEvent, INFINITE);

    // Increment the fence value for the current frame
    m_fenceValues[m_frameIndex]++;
}

void RenderSystem::WaitForFrame(UINT frameIndex) {
    if (m_fence->GetCompletedValue() < m_fenceValues[frameIndex]) {
        HRESULT hr = m_fence->SetEventOnCompletion(m_fenceValues[frameIndex], m_fenceEvent);
        if (FAILED(hr)) {
            throw std::runtime_error("Failed to set fence event");
        }

        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void RenderSystem::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = beforeState;
    barrier.Transition.StateAfter = afterState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    m_commandList->ResourceBarrier(1, &barrier);
}

void RenderSystem::Resize(int width, int height) {
    if (width <= 0 || height <= 0 || !m_device) return;

    // Wait for GPU to finish all work
    WaitForGpu();

    // Update dimensions
    m_width = width;
    m_height = height;
    m_scaledWidth = static_cast<int>(m_width * m_renderScale);
    m_scaledHeight = static_cast<int>(m_height * m_renderScale);

    // Ensure minimum dimensions
    m_scaledWidth = std::max(m_scaledWidth, 1);
    m_scaledHeight = std::max(m_scaledHeight, 1);

    // Release old resources
    for (int i = 0; i < 3; i++) {
        m_renderTargets[i].Reset();
    }

    // Resize swap chain buffers
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    m_swapChain->GetDesc(&swapChainDesc);

    HRESULT hr = m_swapChain->ResizeBuffers(
        3, // Triple buffering
        m_width,
        m_height,
        swapChainDesc.BufferDesc.Format,
        m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
    );

    if (FAILED(hr)) {
        throw std::runtime_error("Failed to resize swap chain buffers");
    }

    // Update frame index
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate render targets
    CreateRenderTargets();
}

void RenderSystem::SetRenderScale(float scale) {
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
}

void RenderSystem::ReleaseResources() {
    // Release render targets
    for (int i = 0; i < 3; i++) {
        m_renderTargets[i].Reset();
    }

    // Let ResourceManager clean up its resources
    if (m_resourceManager) {
        m_resourceManager->ClearCache();
    }
}