// GameOverlay - RenderSystem.h
// Phase 6: DirectX 12 Migration
// DirectX 12 rendering system for the overlay

#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <Windows.h>
#include "PerformanceOptimizer.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class ResourceManager;

// Forward declarations for DirectX 12 helper structures
struct FrameContext;
struct DescriptorHeapManager;

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

    // Getters for DirectX 12 resources (for ImGui integration and other components)
    ID3D12Device* GetDevice() const { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    DescriptorHeapManager* GetDescriptorHeapManager() const { return m_descriptorManager.get(); }
    UINT GetCurrentFrameIndex() const { return m_frameIndex; }

    // DirectX 12 specific functionality
    ID3D12Resource* GetCurrentRenderTarget() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView() const;
    UINT GetCurrentBackBufferIndex() const;
    void WaitForGpu();

private:
    void InitializeDirectX12(HWND hwnd, int width, int height);
    void CreateCommandObjects();
    void CreateSwapChain(HWND hwnd, int width, int height);
    void CreateRenderTargets();
    void CreateSyncObjects();
    void MoveToNextFrame();
    void WaitForFrame(UINT frameIndex);
    void ReleaseResources();

    // DirectX 12 objects
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[3]; // Triple buffering
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Resource> m_renderTargets[3]; // Triple buffering
    std::unique_ptr<DescriptorHeapManager> m_descriptorManager;

    // Synchronization objects
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[3] = { 0, 0, 0 }; // Values for each frame
    HANDLE m_fenceEvent = nullptr;

    // Frame management
    UINT m_frameIndex = 0;
    std::unique_ptr<FrameContext> m_frameContexts[3]; // Triple buffering

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

    // Additional state for DirectX 12
    bool m_useWarpAdapter = false;
    UINT m_rtvDescriptorSize = 0;
    bool m_tearingSupported = false;
    bool m_allowTearing = false;

    // Helper methods for DirectX 12
    void PopulateCommandList();
    void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);
    void CheckTearingSupport();
    void UpdateRenderTargetViews();
};

// Helper structure to manage descriptor heaps
struct DescriptorHeapManager {
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    ComPtr<ID3D12DescriptorHeap> dsvHeap;
    ComPtr<ID3D12DescriptorHeap> cbvSrvUavHeap;
    ComPtr<ID3D12DescriptorHeap> samplerHeap;

    UINT rtvDescriptorSize = 0;
    UINT dsvDescriptorSize = 0;
    UINT cbvSrvUavDescriptorSize = 0;
    UINT samplerDescriptorSize = 0;

    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetCbvSrvUavCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavGpuHandle(UINT index) const;
};

// Per-frame context data
struct FrameContext {
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    UINT64 fenceValue = 0;

    FrameContext(ID3D12Device* device);
    void Reset();
};