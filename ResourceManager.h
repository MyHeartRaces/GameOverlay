// GameOverlay - ResourceManager.h
// Resource pooling and efficient memory management for DirectX 12

#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <functional>
#include <chrono>
#include <unordered_map>

// Forward declarations
class RenderSystem;

using Microsoft::WRL::ComPtr;

// Resource type identifiers
enum class ResourceType {
    Texture,
    Buffer,         // General purpose buffer (Default/Custom heap)
    UploadBuffer,   // Buffer in Upload heap
    ReadbackBuffer, // Buffer in Readback heap
    RenderTarget,
    DepthStencil,
    Shader,         // Consider if needed, or manage via PipelineStateManager
    Other
};

// Resource state tracking for DirectX 12
struct ResourceState {
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    // Removed isTransitioning, less reliable than external sync
};

// Resource usage tracking
struct ResourceUsage {
    std::chrono::steady_clock::time_point lastUsed;
    size_t size = 0;
    bool isPinned = false;
    ResourceType type = ResourceType::Other; // Store type for stats
    // State is now tracked separately by pointer
};

// DirectX 12 resource descriptor handle info
struct ResourceDescriptor {
    UINT heapIndex = UINT_MAX; // Use invalid index marker
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // Default assumption
};

// Resource manager for efficient resource pooling and reuse
class ResourceManager {
public:
    ResourceManager(RenderSystem* renderSystem);
    ~ResourceManager();

    // Disable copy and move
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // --- Texture Resource Management ---
    ComPtr<ID3D12Resource> CreateTexture2D(
        UINT width, UINT height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        const D3D12_CLEAR_VALUE* optimizedClearValue = nullptr); // Add clear value option

    // --- Buffer Resource Management ---

    // Creates a buffer, potentially initializing with data *only if* heapType is UPLOAD.
    // For other heap types, data must be uploaded separately using UpdateBuffer.
    ComPtr<ID3D12Resource> CreateBuffer(
        UINT64 sizeInBytes,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
        const void* initialData = nullptr); // Initial data only for UPLOAD heap

    // Helper to update a buffer (typically Default/Custom heap) using an intermediate upload buffer.
    // Returns the required upload buffer (caller should manage its lifetime or use a temp one).
    void UpdateBuffer(
        ID3D12GraphicsCommandList* commandList,
        ID3D12Resource* destinationBuffer,
        ComPtr<ID3D12Resource>& uploadBuffer, // Reusable upload buffer
        const void* data,
        UINT64 dataSize,
        UINT64 destinationOffset = 0,
        D3D12_RESOURCE_STATES stateAfterCopy = D3D12_RESOURCE_STATE_COMMON); // State after copy is done

    // Specialized buffer creation
    ComPtr<ID3D12Resource> CreateUploadBuffer(UINT64 size);
    ComPtr<ID3D12Resource> CreateConstantBuffer(UINT size); // Uses Upload heap internally

    // --- View Creation ---
    ResourceDescriptor CreateShaderResourceView(
        ID3D12Resource* resource,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN, // Use resource format if unknown
        D3D12_SRV_DIMENSION viewDimension = D3D12_SRV_DIMENSION_UNKNOWN, // Deduce if unknown
        UINT mostDetailedMip = 0, UINT mipLevels = -1); // Add mip control

    ResourceDescriptor CreateRenderTargetView(
        ID3D12Resource* resource,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

    ResourceDescriptor CreateDepthStencilView(
        ID3D12Resource* resource,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

    ResourceDescriptor CreateUnorderedAccessView(
         ID3D12Resource* resource,
         DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN,
         D3D12_UAV_DIMENSION viewDimension = D3D12_UAV_DIMENSION_UNKNOWN,
         ID3D12Resource* counterResource = nullptr); // Basic UAV creation


    // --- Resource Tracking & Cleanup ---
    // Tracks internally created resources. Use TrackExplicitResource for external ones.
    void TrackExplicitResource(const std::string& id, ID3D12Resource* resource, ResourceType type, size_t size,
                                D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
    void ReleaseResource(const std::string& id); // Releases tracking, caller manages actual release
    void ReleaseResource(ID3D12Resource* resource); // Finds and releases tracking
    void ReleaseUnusedResources(std::chrono::seconds maxAge = std::chrono::seconds(60));

    // --- Resource State Management ---
    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource) const;
    void SetResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state); // Use with caution
    void TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
                            D3D12_RESOURCE_STATES newState);
    void TransitionResources(ID3D12GraphicsCommandList* commandList,
                             const std::vector<D3D12_RESOURCE_BARRIER>& barriers); // Take barriers directly
    // Helper to create a transition barrier
    static D3D12_RESOURCE_BARRIER TransitionBarrier(ID3D12Resource* resource,
                                                    D3D12_RESOURCE_STATES stateBefore,
                                                    D3D12_RESOURCE_STATES stateAfter,
                                                    UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);


    // --- Resource Usage & Pooling ---
    void NotifyResourceUsed(ID3D12Resource* resource);
    void PinResource(ID3D12Resource* resource, bool pin);
    bool IsPinned(ID3D12Resource* resource) const;

    // --- Descriptor Management ---
    // Consider a more robust DescriptorHeap class if complexity grows
    ResourceDescriptor AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void FreeDescriptor(const ResourceDescriptor& descriptor);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptorHandle(const ResourceDescriptor& descriptor) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(const ResourceDescriptor& descriptor) const;
    UINT GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;

    // --- Memory Statistics ---
    size_t GetTotalMemoryUsage() const;
    size_t GetMemoryUsageByType(ResourceType type) const;

    // --- Cache Management ---
    void ClearCache(); // Clears internal tracking, doesn't release resources directly
    void SetCacheLimit(size_t maxMemorySizeBytes); // Influences ReleaseUnusedResources

private:
    // Internal tracking for resources created by the manager
    void TrackResourceInternal(ID3D12Resource* resource, ResourceType type, size_t size,
                              D3D12_RESOURCE_STATES initialState);

    // Resource usage tracking map (using pointer as key is faster for lookups)
    std::unordered_map<ID3D12Resource*, ResourceUsage> m_resourceUsage;
    // Resource state tracking map
    std::unordered_map<ID3D12Resource*, ResourceState> m_resourceStates;
    // Optional: Map string ID to resource pointer if needed for external access
    std::unordered_map<std::string, ID3D12Resource*> m_namedResources;

    // Total memory usage by type
    std::map<ResourceType, size_t> m_memoryUsageByType;

    // Descriptor management (Simplified - consider dedicated class)
    struct DescriptorPool {
        std::vector<bool> allocated; // Simple allocation tracking
        UINT capacity = 0;
        UINT size = 0; // Number currently allocated
        D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        UINT descriptorSize = 0;
        ComPtr<ID3D12DescriptorHeap> heap; // Store heap pointer
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart = {};
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = {};
    };
    std::map<D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorPool> m_descriptorPools;
    void InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool shaderVisible);


    // Configuration
    size_t m_maxCacheSize = 256 * 1024 * 1024; // 256 MB default limit for auto-release

    // Thread safety
    mutable std::mutex m_mutex;

    // Resource pointer (not owned)
    RenderSystem* m_renderSystem = nullptr;

    // Helper method to get resource ID (if tracked by name)
    std::string GetResourceID(ID3D12Resource* resource) const;
};
