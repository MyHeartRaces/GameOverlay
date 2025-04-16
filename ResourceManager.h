// GameOverlay - ResourceManager.h
// Phase 5: Performance Optimization
// Resource pooling and efficient memory management

#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <mutex>
#include <functional>
#include <chrono>

// Forward declarations
class RenderSystem;

// Resource type identifiers
enum class ResourceType {
    Texture,
    Buffer,
    Shader,
    RenderTarget,
    Other
};

// Resource usage tracking
struct ResourceUsage {
    std::chrono::steady_clock::time_point lastUsed;
    size_t size = 0;
    bool isPinned = false;
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
    Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture2D(
        UINT width, UINT height,
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM,
        UINT bindFlags = D3D11_BIND_SHADER_RESOURCE,
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT);

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateShaderResourceView(
        ID3D11Texture2D* texture);

    // Buffer resource management
    template<typename T>
    Microsoft::WRL::ComPtr<ID3D11Buffer> CreateBuffer(
        const T* data, UINT count,
        D3D11_BIND_FLAG bindFlags = D3D11_BIND_VERTEX_BUFFER,
        D3D11_USAGE usage = D3D11_USAGE_DEFAULT);

    // Resource tracking and cleanup
    void TrackResource(const std::string& id, void* resource, ResourceType type, size_t size);
    void ReleaseResource(const std::string& id);
    void ReleaseUnusedResources(std::chrono::seconds maxAge = std::chrono::seconds(60));

    // Resource usage notification
    void NotifyResourceUsed(const std::string& id);

    // Resource pool management
    void PinResource(const std::string& id, bool pin);
    bool IsPinned(const std::string& id) const;

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
        D3D11_USAGE usage;
        UINT bindFlags;
    };

    // Typedefs for resource caches
    using TextureCache = std::vector<CachedResource<ID3D11Texture2D>>;
    using BufferCache = std::vector<CachedResource<ID3D11Buffer>>;

    // Resource caches
    TextureCache m_textureCache;
    BufferCache m_bufferCache;

    // Resource usage tracking
    std::map<std::string, ResourceUsage> m_resourceUsage;

    // Total memory usage by type
    std::map<ResourceType, size_t> m_memoryUsageByType;

    // Configuration
    size_t m_maxCacheSize = 256 * 1024 * 1024; // 256 MB by default

    // Resource cleanup callback
    using CleanupCallback = std::function<void(void*)>;
    std::map<std::string, CleanupCallback> m_cleanupCallbacks;

    // Thread safety
    mutable std::mutex m_mutex;

    // Resource pointer (not owned)
    RenderSystem* m_renderSystem = nullptr;

    // Helper methods
    bool TryFindCachedTexture(UINT width, UINT height, DXGI_FORMAT format,
        UINT bindFlags, D3D11_USAGE usage,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture);

    template<typename T>
    bool TryFindCachedBuffer(UINT byteSize, UINT bindFlags, D3D11_USAGE usage,
        Microsoft::WRL::ComPtr<ID3D11Buffer>& outBuffer);

    // Cache management helpers
    void TrimCache();
    void UpdateResourceUsage(const std::string& id, size_t size);
};