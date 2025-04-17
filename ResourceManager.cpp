// GameOverlay - ResourceManager.cpp
// Phase 6: DirectX 12 Migration
// Resource pooling and efficient memory management for DirectX 12

#include "ResourceManager.h"
#include "RenderSystem.h"
#include <algorithm>
#include <sstream>

ResourceManager::ResourceManager(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem) {

    // Initialize descriptor pools
    DescriptorPool rtvPool;
    rtvPool.capacity = 100;
    rtvPool.allocated.resize(rtvPool.capacity, false);
    m_descriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_RTV] = rtvPool;

    DescriptorPool dsvPool;
    dsvPool.capacity = 50;
    dsvPool.allocated.resize(dsvPool.capacity, false);
    m_descriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_DSV] = dsvPool;

    DescriptorPool cbvSrvUavPool;
    cbvSrvUavPool.capacity = 1000;
    cbvSrvUavPool.allocated.resize(cbvSrvUavPool.capacity, false);
    m_descriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV] = cbvSrvUavPool;

    DescriptorPool samplerPool;
    samplerPool.capacity = 50;
    samplerPool.allocated.resize(samplerPool.capacity, false);
    m_descriptorPools[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER] = samplerPool;
}

ResourceManager::~ResourceManager() {
    // Clean up all resources
    ClearCache();
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceManager::CreateTexture2D(
    UINT width, UINT height,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> texture;

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (TryFindCachedTexture(width, height, format, flags, heapType, texture)) {
            // Update state tracking
            SetResourceState(texture.Get(), initialState);
            return texture;
        }
    }

    // Create texture description
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = flags;

    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // Create texture
    HRESULT hr = m_renderSystem->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&texture)
    );

    if (FAILED(hr)) {
        return nullptr;
    }

    // Calculate resource size (estimate)
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
        "_" + std::to_string(format) + "_" + std::to_string(static_cast<int>(flags)) +
        "_" + std::to_string(reinterpret_cast<uintptr_t>(texture.Get()));

    // Track the resource
    TrackResource(id, texture.Get(), ResourceType::Texture, resourceSize, initialState);

    return texture;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceManager::CreateUploadBuffer(UINT64 size) {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // Create resource description
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // Create buffer
    HRESULT hr = m_renderSystem->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, // Upload buffers are always in GENERIC_READ state
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr)) {
        return nullptr;
    }

    // Create unique ID for tracking
    std::string id = "UploadBuffer_" + std::to_string(size) +
        "_" + std::to_string(reinterpret_cast<uintptr_t>(buffer.Get()));

    // Track the resource
    TrackResource(id, buffer.Get(), ResourceType::UploadBuffer, size, D3D12_RESOURCE_STATE_GENERIC_READ);

    return buffer;
}

ResourceDescriptor ResourceManager::CreateShaderResourceView(
    ID3D12Resource* resource,
    DXGI_FORMAT format) {

    if (!m_renderSystem || !m_renderSystem->GetDevice() || !resource) {
        return {};
    }

    ResourceDescriptor descriptor = {};
    descriptor.type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

    // Allocate descriptor from the heap
    descriptor.heapIndex = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    descriptor.cpuHandle = GetCpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor.heapIndex);
    descriptor.gpuHandle = GetGpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, descriptor.heapIndex);

    // Get resource description
    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();

    // Create SRV description
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = (format != DXGI_FORMAT_UNKNOWN) ? format : resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(resourceDesc.Width / 4); // Assuming 4 bytes per element
        srvDesc.Buffer.StructureByteStride = 0;
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
    }

    // Create the SRV
    m_renderSystem->GetDevice()->CreateShaderResourceView(resource, &srvDesc, descriptor.cpuHandle);

    return descriptor;
}

Microsoft::WRL::ComPtr<ID3D12Resource> ResourceManager::CreateConstantBuffer(
    UINT size,
    D3D12_RESOURCE_STATES initialState) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    // Constant buffers must be a multiple of 256 bytes
    UINT alignedSize = (size + 255) & ~255;

    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // Constant buffers should be in upload heap
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // Create resource description
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = alignedSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;

    // Create buffer
    HRESULT hr = m_renderSystem->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&constantBuffer)
    );

    if (FAILED(hr)) {
        return nullptr;
    }

    // Create unique ID for tracking
    std::string id = "ConstantBuffer_" + std::to_string(alignedSize) +
        "_" + std::to_string(reinterpret_cast<uintptr_t>(constantBuffer.Get()));

    // Track the resource
    TrackResource(id, constantBuffer.Get(), ResourceType::Buffer, alignedSize, initialState);

    return constantBuffer;
}

template<typename T>
Microsoft::WRL::ComPtr<ID3D12Resource> ResourceManager::CreateBuffer(
    const T* data, UINT count,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;

    UINT64 bufferSize = count * sizeof(T);

    // Check cache first
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (TryFindCachedBuffer<T>(bufferSize, flags, heapType, buffer)) {
            // Update resource state
            SetResourceState(buffer.Get(), initialState);
            return buffer;
        }
    }

    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // Create resource description
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = bufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = flags;

    // Create destination buffer
    HRESULT hr = m_renderSystem->GetDevice()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState,
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr)) {
        return nullptr;
    }

    // If data is provided and destination is not an upload heap, create upload buffer and copy data
    if (data && heapType != D3D12_HEAP_TYPE_UPLOAD) {
        // Create upload buffer
        Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer = CreateUploadBuffer(bufferSize);

        if (uploadBuffer) {
            // Map upload buffer and copy data
            UINT8* mappedData = nullptr;
            D3D12_RANGE readRange = { 0, 0 }; // We're not reading

            hr = uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
            if (SUCCEEDED(hr)) {
                memcpy(mappedData, data, bufferSize);
                uploadBuffer->Unmap(0, nullptr);

                // Copy from upload buffer to destination buffer
                // This should be done using command list, but for simplicity
                // we'll assume the transfer will be managed elsewhere
            }
        }
    }
    // If data is provided and destination is an upload heap, copy directly
    else if (data && heapType == D3D12_HEAP_TYPE_UPLOAD) {
        // Map destination buffer and copy data
        UINT8* mappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 }; // We're not reading

        hr = buffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData));
        if (SUCCEEDED(hr)) {
            memcpy(mappedData, data, bufferSize);
            buffer->Unmap(0, nullptr);
        }
    }

    // Create unique ID for tracking
    std::string id = "Buffer_" + std::to_string(bufferSize) +
        "_" + std::to_string(static_cast<int>(flags)) +
        "_" + std::to_string(reinterpret_cast<uintptr_t>(buffer.Get()));

    // Track the resource
    TrackResource(id, buffer.Get(), ResourceType::Buffer, bufferSize, initialState);

    return buffer;
}

void ResourceManager::TrackResource(const std::string& id, ID3D12Resource* resource, ResourceType type, size_t size,
    D3D12_RESOURCE_STATES initialState) {

    std::lock_guard<std::mutex> lock(m_mutex);

    ResourceUsage usage;
    usage.lastUsed = std::chrono::steady_clock::now();
    usage.size = size;
    usage.isPinned = false;
    usage.state.currentState = initialState;
    usage.state.isTransitioning = false;

    m_resourceUsage[id] = usage;

    // Also track state by pointer for faster lookup
    m_resourceStates[resource] = usage.state;

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

D3D12_RESOURCE_STATES ResourceManager::GetResourceState(ID3D12Resource* resource) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceStates.find(resource);
    if (it != m_resourceStates.end()) {
        return it->second.currentState;
    }

    // Default state if not found
    return D3D12_RESOURCE_STATE_COMMON;
}

void ResourceManager::SetResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceStates.find(resource);
    if (it != m_resourceStates.end()) {
        it->second.currentState = state;
    }
    else {
        ResourceState resourceState;
        resourceState.currentState = state;
        resourceState.isTransitioning = false;
        m_resourceStates[resource] = resourceState;
    }
}

void ResourceManager::TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES newState) {

    if (!commandList || !resource) {
        return;
    }

    D3D12_RESOURCE_STATES currentState = GetResourceState(resource);

    // Skip if already in the desired state
    if (currentState == newState) {
        return;
    }

    // Create barrier
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = currentState;
    barrier.Transition.StateAfter = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    // Apply barrier
    commandList->ResourceBarrier(1, &barrier);

    // Update state
    SetResourceState(resource, newState);
}

void ResourceManager::TransitionResources(ID3D12GraphicsCommandList* commandList,
    const std::vector<std::pair<ID3D12Resource*, D3D12_RESOURCE_STATES>>& transitions) {

    if (!commandList || transitions.empty()) {
        return;
    }

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(transitions.size());

    // Create barriers
    for (const auto& [resource, newState] : transitions) {
        D3D12_RESOURCE_STATES currentState = GetResourceState(resource);

        // Skip if already in the desired state
        if (currentState == newState) {
            continue;
        }

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource;
        barrier.Transition.StateBefore = currentState;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        barriers.push_back(barrier);

        // Update state
        SetResourceState(resource, newState);
    }

    // Apply barriers
    if (!barriers.empty()) {
        commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
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

UINT ResourceManager::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& pool = m_descriptorPools[type];

    // Find first free descriptor
    for (UINT i = 0; i < pool.capacity; i++) {
        if (!pool.allocated[i]) {
            pool.allocated[i] = true;
            pool.size++;
            return i;
        }
    }

    // If we get here, all descriptors are in use
    // In a real implementation, you would resize the descriptor heap
    // For now, we'll just return an invalid index
    return UINT_MAX;
}

void ResourceManager::FreeDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& pool = m_descriptorPools[type];

    if (index < pool.capacity && pool.allocated[index]) {
        pool.allocated[index] = false;
        pool.size--;
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetCpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) const {
    // This requires access to DescriptorHeapManager from RenderSystem
    // For simplicity, we'll assume we have access to the appropriate descriptor heap
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {};

    if (m_renderSystem && m_renderSystem->GetDescriptorHeapManager()) {
        auto manager = m_renderSystem->GetDescriptorHeapManager();

        switch (type) {
        case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
            handle = manager->GetRtvHandle(index);
            break;

        case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
            handle = manager->GetDsvHandle(index);
            break;

        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            handle = manager->GetCbvSrvUavCpuHandle(index);
            break;
        }
    }

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::GetGpuDescriptorHandle(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT index) const {
    // Only CBV/SRV/UAV and Sampler heaps can have GPU handles
    if (type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER) {
        return {};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE handle = {};

    if (m_renderSystem && m_renderSystem->GetDescriptorHeapManager()) {
        auto manager = m_renderSystem->GetDescriptorHeapManager();

        if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) {
            handle = manager->GetCbvSrvUavGpuHandle(index);
        }
    }

    return handle;
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

    // Clear all collections
    m_resourceUsage.clear();
    m_resourceStates.clear();
    m_memoryUsageByType.clear();
    m_textureCache.clear();
    m_bufferCache.clear();

    // Reset descriptor pools
    for (auto& [type, pool] : m_descriptorPools) {
        std::fill(pool.allocated.begin(), pool.allocated.end(), false);
        pool.size = 0;
    }
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
    D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outTexture) {

    // Find a suitable texture in the cache
    auto it = std::find_if(m_textureCache.begin(), m_textureCache.end(),
        [width, height, format, flags, heapType](const CachedResource<ID3D12Resource>& cached) {
            return cached.width == width &&
                cached.height == height &&
                cached.format == format &&
                cached.flags == flags &&
                cached.heapType == heapType;
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
bool ResourceManager::TryFindCachedBuffer(UINT64 sizeInBytes, D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType,
    Microsoft::WRL::ComPtr<ID3D12Resource>& outBuffer) {

    // Find a suitable buffer in the cache
    auto it = std::find_if(m_bufferCache.begin(), m_bufferCache.end(),
        [sizeInBytes, flags, heapType](const CachedResource<ID3D12Resource>& cached) {
            return cached.size >= sizeInBytes && // Size must be at least as large
                cached.flags == flags &&
                cached.heapType == heapType;
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

std::string ResourceManager::GetResourceID(ID3D12Resource* resource) const {
    for (const auto& [id, usage] : m_resourceUsage) {
        // This would require mapping resources to IDs
        // In a real implementation, you'd store this mapping
    }

    // If not found, return empty string
    return "";
}
