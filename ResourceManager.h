// GameOverlay - ResourceManager.h
// Phase 6: DirectX 12 Migration
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

// Resource type identifiers
enum class ResourceType {
    Texture,
    Buffer,
    UploadBuffer,
    Shader,
    RenderTarget,
    Other
};

// Resource state tracking for DirectX 12
struct ResourceState {
    D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
    bool isTransitioning = false;
};

// Resource usage tracking
struct ResourceUsage {
    std::chrono::steady_clock::time_point lastUsed;
    size_t size = 0;
    bool isPinned = false;
    ResourceState state;
};

// DirectX 12 resource descriptor
struct ResourceDescriptor {
    UINT heapIndex = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
    D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
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

    // Texture resource management
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateTexture2D(
        UINT width, UINT height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

    // Upload buffer for texture data
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(
        UINT64 size);

    // Create shader resource view for texture
    ResourceDescriptor CreateShaderResourceView(
        ID3D12Resource* resource,
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN);

    // Buffer resource management
    template<typename T>
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateBuffer(
        const T* data, UINT count,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);

    // Constant buffer management
    Microsoft::WRL::ComPtr<ID3D12Resource> CreateConstantBuffer(
        UINT size,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ);

    // Resource tracking and cleanup
    void TrackResource(const std::string& id, ID3D12Resource* resource, ResourceType type, size_t size,
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
    void ReleaseResource(const std::string& id);
    void ReleaseUnusedResources(std::chrono::seconds maxAge = std::chrono::seconds(60));

    // Resource state tracking
    D3D12_RESOURCE_STATES GetResourceState(ID3D12Resource* resource) const;
    void SetResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state);
    void TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
        D3D12_RESOURCE_STATES newState);
    void TransitionResources(ID3D12GraphicsCommandList* commandList,
        const std::vector<std::pair<ID3D12Resource*, D3D12_RESOURCE_STATES>>& transitions);

    // Resource usage notification
    void NotifyResourceUsed(const std::string& id);

    // Resource pool management
    void PinResource(const std::string& id, bool pin);
    bool IsPinned(const std::string& id) const;

    // Descriptor management
    UINT AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type);
    void FreeDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) const;

    // Memory statistics
    size_t GetTotalMemoryUsage() const;
    size_t GetMemoryUsageByType(ResourceType type) const;

    // Resource cache management
    void ClearCache();
    void SetCacheLimit(size_t maxMemorySizeBytes);

private:
    // Resource caching
    template<typename ResourceT>
    struct CachedResource {
        Microsoft::WRL::ComPtr<ResourceT> resource;
        std::chrono::steady_clock::time_point lastUsed;
        size_t size;
        UINT width, height;
        DXGI_FORMAT format;
        D3D12_HEAP_TYPE heapType;
        D3D12_RESOURCE_FLAGS flags;
    };

    // Typedefs for resource caches
    using TextureCache = std::vector<CachedResource<ID3D12Resource>>;
    using BufferCache = std::vector<CachedResource<ID3D12Resource>>;

    // Resource caches
    TextureCache m_textureCache;
    BufferCache m_bufferCache;

    // Resource usage tracking
    std::map<std::string, ResourceUsage> m_resourceUsage;
    std::unordered_map<ID3D12Resource*, ResourceState> m_resourceStates;

    // Total memory usage by type
    std::map<ResourceType, size_t> m_memoryUsageByType;

    // Descriptor management
    struct DescriptorPool {
        std::vector<bool> allocated;
        UINT capacity = 0;
        UINT size = 0;
    };
    std::map<D3D12_DESCRIPTOR_HEAP_TYPE, DescriptorPool> m_descriptorPools;

    // Configuration
    size_t m_maxCacheSize = 256 * 1024 * 1024; // 256 MB by default

    // Resource cleanup callback
    using CleanupCallback = std::function<void(void*)>;
    std::map<std::string, CleanupCallback> m_cleanupCallbacks;

    // Thread safety
    mutable std::mutex m_mutex;

    // Resource pointer (not owned)
    RenderSystem* m_renderSystem = nullptr;

    // Helper methods for resource caching
    bool TryFindCachedTexture(UINT width, UINT height, DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outTexture);

    template<typename T>
    bool TryFindCachedBuffer(UINT64 sizeInBytes, D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType,
        Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer);

    // Cache management helpers
    void TrimCache();
    void UpdateResourceUsage(const std::string& id, size_t size);

    // Helper method to get resource ID
    std::string GetResourceID(ID3D12Resource* resource) const;
};