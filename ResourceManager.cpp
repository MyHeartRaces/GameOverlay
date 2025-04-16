// GameOverlay - ResourceManager.cpp
// Phase 5: Performance Optimization
// Resource pooling and efficient memory management

#include "ResourceManager.h"
#include "RenderSystem.h"
#include <algorithm>

ResourceManager::ResourceManager(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem) {
}

ResourceManager::~ResourceManager() {
    // Clean up all resources
    ClearCache();
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> ResourceManager::CreateTexture2D(
    UINT width, UINT height,
    DXGI_FORMAT format,
    UINT bindFlags,
    D3D11_USAGE usage) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (TryFindCachedTexture(width, height, format, bindFlags, usage, texture)) {
            return texture;
        }
    }

    // Create new texture if not found in cache
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = usage;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING) ?
        D3D11_CPU_ACCESS_WRITE : 0;
    desc.MiscFlags = 0;

    HRESULT hr = m_renderSystem->GetDevice()->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr)) {
        return nullptr;
    }

    // Calculate size (estimate)
    size_t bytesPerPixel = 4; // Default for RGBA8
    switch (format) {
    case DXGI_FORMAT_R8_UNORM:
        bytesPerPixel = 1;
        break;
    case DXGI_FORMAT_R8G8_UNORM:
        bytesPerPixel = 2;
        break;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        bytesPerPixel = 8;
        break;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        bytesPerPixel = 16;
        break;
        // Add more formats as needed
    }

    size_t resourceSize = width * height * bytesPerPixel;

    // Create unique ID for tracking
    std::string id = "Texture_" + std::to_string(width) + "x" + std::to_string(height) +
        "_" + std::to_string(format) + "_" + std::to_string(bindFlags) +
        "_" + std::to_string(reinterpret_cast<uintptr_t>(texture.Get()));

    // Track the resource
    TrackResource(id, texture.Get(), ResourceType::Texture, resourceSize);

    return texture;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ResourceManager::CreateShaderResourceView(
    ID3D11Texture2D* texture) {

    if (!m_renderSystem || !m_renderSystem->GetDevice() || !texture) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    // Get texture description
    D3D11_TEXTURE2D_DESC textureDesc;
    texture->GetDesc(&textureDesc);

    // Create shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    HRESULT hr = m_renderSystem->GetDevice()->CreateShaderResourceView(texture, &srvDesc, &srv);
    if (FAILED(hr)) {
        return nullptr;
    }

    // Create unique ID for tracking
    std::string id = "SRV_" + std::to_string(reinterpret_cast<uintptr_t>(srv.Get()));

    // Track the resource (minimal size as it references the texture)
    TrackResource(id, srv.Get(), ResourceType::Other, sizeof(void*));

    return srv;
}

template<typename T>
Microsoft::WRL::ComPtr<ID3D11Buffer> ResourceManager::CreateBuffer(
    const T* data, UINT count,
    D3D11_BIND_FLAG bindFlags,
    D3D11_USAGE usage) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
    UINT byteSize = count * sizeof(T);

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (TryFindCachedBuffer<T>(byteSize, bindFlags, usage, buffer)) {
            return buffer;
        }
    }

    // Create buffer description
    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = usage;
    bufferDesc.ByteWidth = byteSize;
    bufferDesc.BindFlags = bindFlags;
    bufferDesc.CPUAccessFlags = (usage == D3D11_USAGE_DYNAMIC || usage == D3D11_USAGE_STAGING) ?
        D3D11_CPU_ACCESS_WRITE : 0;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = sizeof(T);

    // Create with initial data if provided
    D3D11_SUBRESOURCE_DATA initialData = {};
    initialData.pSysMem = data;

    HRESULT hr = m_renderSystem->GetDevice()->CreateBuffer(
        &bufferDesc,
        (data != nullptr) ? &initialData : nullptr,
        &buffer);

    if (FAILED(hr)) {
        return nullptr;
    }

    // Create unique ID for tracking
    std::string id = "Buffer_" + std::to_string(byteSize) + "_" +
        std::to_string(bindFlags) + "_" + std::to_string(reinterpret_cast<uintptr_t>(buffer.Get()));

    // Track the resource
    TrackResource(id, buffer.Get(), ResourceType::Buffer, byteSize);

    return buffer;
}

void ResourceManager::TrackResource(const std::string& id, void* resource, ResourceType type, size_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ResourceUsage usage;
    usage.lastUsed = std::chrono::steady_clock::now();
    usage.size = size;
    usage.isPinned = false;

    m_resourceUsage[id] = usage;

    // Update memory usage statistics
    m_memoryUsageByType[type] += size;

    // Check if we need to trim cache
    if (GetTotalMemoryUsage() > m_maxCacheSize) {
        TrimCache();
    }
}

void ResourceManager::ReleaseResource(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceUsage.find(id);
    if (it != m_resourceUsage.end()) {
        // Execute cleanup callback if registered
        auto callbackIt = m_cleanupCallbacks.find(id);
        if (callbackIt != m_cleanupCallbacks.end() && callbackIt->second) {
            callbackIt->second(nullptr);
            m_cleanupCallbacks.erase(callbackIt);
        }

        // Remove from usage tracking
        m_resourceUsage.erase(it);
    }
}

void ResourceManager::ReleaseUnusedResources(std::chrono::seconds maxAge) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> toRemove;

    // Find unused resources
    for (const auto& [id, usage] : m_resourceUsage) {
        // Skip pinned resources
        if (usage.isPinned) {
            continue;
        }

        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - usage.lastUsed);
        if (age > maxAge) {
            toRemove.push_back(id);
        }
    }

    // Remove them
    for (const auto& id : toRemove) {
        ReleaseResource(id);
    }
}

void ResourceManager::NotifyResourceUsed(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceUsage.find(id);
    if (it != m_resourceUsage.end()) {
        it->second.lastUsed = std::chrono::steady_clock::now();
    }
}

void ResourceManager::PinResource(const std::string& id, bool pin) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceUsage.find(id);
    if (it != m_resourceUsage.end()) {
        it->second.isPinned = pin;
    }
}

bool ResourceManager::IsPinned(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceUsage.find(id);
    if (it != m_resourceUsage.end()) {
        return it->second.isPinned;
    }

    return false;
}

size_t ResourceManager::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    size_t total = 0;
    for (const auto& [type, size] : m_memoryUsageByType) {
        total += size;
    }

    return total;
}

size_t ResourceManager::GetMemoryUsageByType(ResourceType type) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_memoryUsageByType.find(type);
    if (it != m_memoryUsageByType.end()) {
        return it->second;
    }

    return 0;
}

void ResourceManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Execute all cleanup callbacks
    for (const auto& [id, callback] : m_cleanupCallbacks) {
        if (callback) {
            callback(nullptr);
        }
    }

    // Clear all collections
    m_resourceUsage.clear();
    m_cleanupCallbacks.clear();
    m_memoryUsageByType.clear();
    m_textureCache.clear();
    m_bufferCache.clear();
}

void ResourceManager::SetCacheLimit(size_t maxMemorySizeBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_maxCacheSize = maxMemorySizeBytes;

    // If current usage exceeds new limit, trim cache
    if (GetTotalMemoryUsage() > m_maxCacheSize) {
        TrimCache();
    }
}

bool ResourceManager::TryFindCachedTexture(UINT width, UINT height, DXGI_FORMAT format,
    UINT bindFlags, D3D11_USAGE usage,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture) {
    // Find a suitable texture in the cache
    auto it = std::find_if(m_textureCache.begin(), m_textureCache.end(),
        [width, height, format, bindFlags, usage](const CachedResource<ID3D11Texture2D>& cached) {
            return cached.width == width &&
                cached.height == height &&
                cached.format == format &&
                cached.bindFlags == bindFlags &&
                cached.usage == usage;
        });

    if (it != m_textureCache.end()) {
        outTexture = it->resource;
        it->lastUsed = std::chrono::steady_clock::now();

        // Remove from cache so it's not reused again
        m_textureCache.erase(it);

        return true;
    }

    return false;
}

template<typename T>
bool ResourceManager::TryFindCachedBuffer(UINT byteSize, UINT bindFlags, D3D11_USAGE usage,
    Microsoft::WRL::ComPtr<ID3D11Buffer>& outBuffer) {
    // Find a suitable buffer in the cache
    auto it = std::find_if(m_bufferCache.begin(), m_bufferCache.end(),
        [byteSize, bindFlags, usage](const CachedResource<ID3D11Buffer>& cached) {
            return cached.size >= byteSize && // Size must be at least as large
                cached.bindFlags == bindFlags &&
                cached.usage == usage;
        });

    if (it != m_bufferCache.end()) {
        outBuffer = it->resource;
        it->lastUsed = std::chrono::steady_clock::now();

        // Remove from cache so it's not reused again
        m_bufferCache.erase(it);

        return true;
    }

    return false;
}

void ResourceManager::TrimCache() {
    // Calculate how much we need to free
    size_t currentUsage = GetTotalMemoryUsage();
    if (currentUsage <= m_maxCacheSize) {
        return;
    }

    size_t needToFree = currentUsage - m_maxCacheSize + (m_maxCacheSize / 10); // Free a bit extra

    // Find unpinned resources that haven't been used in a while
    std::vector<std::pair<std::string, std::chrono::steady_clock::time_point>> resources;
    for (const auto& [id, usage] : m_resourceUsage) {
        if (!usage.isPinned) {
            resources.push_back({ id, usage.lastUsed });
        }
    }

    // Sort by last used time (oldest first)
    std::sort(resources.begin(), resources.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // Free resources until we hit the target
    size_t freed = 0;
    for (const auto& [id, lastUsed] : resources) {
        auto it = m_resourceUsage.find(id);
        if (it != m_resourceUsage.end()) {
            freed += it->second.size;
            ReleaseResource(id);

            if (freed >= needToFree) {
                break;
            }
        }
    }
}

void ResourceManager::UpdateResourceUsage(const std::string& id, size_t size) {
    auto it = m_resourceUsage.find(id);
    if (it != m_resourceUsage.end()) {
        // Update last used time
        it->second.lastUsed = std::chrono::steady_clock::now();

        // Update size if changed
        if (it->second.size != size) {
            it->second.size = size;
        }
    }
}
