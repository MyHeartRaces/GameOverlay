// GameOverlay - ResourceManager.cpp
// Resource pooling and efficient memory management for DirectX 12

#include "ResourceManager.h"
#include "RenderSystem.h" // Assuming RenderSystem provides GetDevice()
#include <algorithm>
#include <sstream>
#include <stdexcept>

// Helper to generate a somewhat unique ID string based on pointer
std::string PtrToID(const void* ptr) {
    std::stringstream ss;
    ss << ptr;
    return ss.str();
}


ResourceManager::ResourceManager(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem) {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        throw std::runtime_error("ResourceManager requires a valid RenderSystem and D3D12Device.");
    }

    // Initialize descriptor pools
    InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 128, false);         // Render Target Views
    InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, false);          // Depth Stencil Views
    InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048, true); // Shader Resources (needs to be large)
    InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 128, true);      // Samplers
}

ResourceManager::~ResourceManager() {
    // Destructor doesn't release resources here. Assume external management or
    // that ComPtrs holding the actual resources are released elsewhere.
    // ClearCache just clears tracking maps.
    ClearCache();
}

void ResourceManager::InitializeDescriptorPool(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool shaderVisible) {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) return;
    ID3D12Device* device = m_renderSystem->GetDevice();

    DescriptorPool pool;
    pool.capacity = capacity;
    pool.allocated.resize(capacity, false);
    pool.size = 0;
    pool.type = type;
    pool.descriptorSize = device->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = capacity;
    heapDesc.Type = type;
    heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heapDesc.NodeMask = 0; // Single adapter

    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&pool.heap));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create descriptor heap of type " + std::to_string(type));
    }
    pool.heap->SetName((L"ResourceManager Descriptor Heap Type " + std::to_wstring(type)).c_str());

    pool.cpuStart = pool.heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible) {
        pool.gpuStart = pool.heap->GetGPUDescriptorHandleForHeapStart();
    }
    else {
        pool.gpuStart = {}; // Invalid handle
    }

    m_descriptorPools[type] = std::move(pool); // Move pool into map
}


// --- Texture Resource Management ---

ComPtr<ID3D12Resource> ResourceManager::CreateTexture2D(
    UINT width, UINT height,
    DXGI_FORMAT format,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState,
    const D3D12_CLEAR_VALUE* optimizedClearValue) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }
    ID3D12Device* device = m_renderSystem->GetDevice();

    // Create texture description
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1; // Simplify: Only 1 mip level for now
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
    heapProps.CreationNodeMask = 1; // Single GPU
    heapProps.VisibleNodeMask = 1; // Single GPU

    ComPtr<ID3D12Resource> texture;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        initialState,
        optimizedClearValue, // Pass clear value
        IID_PPV_ARGS(&texture)
    );

    if (FAILED(hr)) {
        // Log error HRESULT hr
        OutputDebugStringA(("Failed to create Texture2D. HRESULT: " + std::to_string(hr) + "\n").c_str());
        return nullptr;
    }

    // Calculate approximate size for tracking
    // This is complex; use GetResourceAllocationInfo for accuracy if needed
    D3D12_RESOURCE_ALLOCATION_INFO allocInfo = device->GetResourceAllocationInfo(0, 1, &textureDesc);
    size_t resourceSize = allocInfo.SizeInBytes;

    // Track the internally created resource
    ResourceType resType = ResourceType::Texture;
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) resType = ResourceType::RenderTarget;
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) resType = ResourceType::DepthStencil;

    TrackResourceInternal(texture.Get(), resType, resourceSize, initialState);
    texture->SetName((L"Texture2D_" + std::to_wstring(width) + L"x" + std::to_wstring(height)).c_str());


    return texture;
}

// --- Buffer Resource Management ---

ComPtr<ID3D12Resource> ResourceManager::CreateBuffer(
    UINT64 sizeInBytes,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_HEAP_TYPE heapType,
    D3D12_RESOURCE_STATES initialState,
    const void* initialData) {

    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }
    ID3D12Device* device = m_renderSystem->GetDevice();

    // Data can only be provided directly if target heap is UPLOAD
    if (initialData && heapType != D3D12_HEAP_TYPE_UPLOAD) {
        OutputDebugStringA("Warning: Initial data provided for non-UPLOAD heap buffer. Data will NOT be uploaded by CreateBuffer. Use UpdateBuffer.\n");
        initialData = nullptr; // Ignore data for non-upload heaps in this function
    }

    // Create heap properties
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1; // Single GPU
    heapProps.VisibleNodeMask = 1; // Single GPU

    // Create resource description
    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = sizeInBytes;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = flags;

    ComPtr<ID3D12Resource> buffer;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        initialState, // Initial state for the target heap
        nullptr,
        IID_PPV_ARGS(&buffer)
    );

    if (FAILED(hr)) {
        OutputDebugStringA(("Failed to create Buffer. HRESULT: " + std::to_string(hr) + "\n").c_str());
        return nullptr;
    }
    buffer->SetName((L"Buffer_" + std::to_wstring(sizeInBytes)).c_str());


    // Handle initial data for UPLOAD heap
    if (initialData && heapType == D3D12_HEAP_TYPE_UPLOAD) {
        void* mappedData = nullptr;
        D3D12_RANGE readRange = { 0, 0 }; // We are writing, not reading
        hr = buffer->Map(0, &readRange, &mappedData);
        if (SUCCEEDED(hr)) {
            memcpy(mappedData, initialData, sizeInBytes);
            // No need to specify write range when unmapping UPLOAD heap
            buffer->Unmap(0, nullptr);
        }
        else {
            OutputDebugStringA(("Warning: Failed to map UPLOAD buffer for initial data copy. HRESULT: " + std::to_string(hr) + "\n").c_str());
        }
    }

    // Track the resource
    ResourceType resType = (heapType == D3D12_HEAP_TYPE_UPLOAD) ? ResourceType::UploadBuffer : ResourceType::Buffer;
    TrackResourceInternal(buffer.Get(), resType, sizeInBytes, initialState);

    return buffer;
}


void ResourceManager::UpdateBuffer(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* destinationBuffer,
    ComPtr<ID3D12Resource>& uploadBuffer, // Pass by ref to potentially reuse/create
    const void* data,
    UINT64 dataSize,
    UINT64 destinationOffset,
    D3D12_RESOURCE_STATES stateAfterCopy) {

    if (!commandList || !destinationBuffer || !data || dataSize == 0) {
        throw std::invalid_argument("Invalid argument for UpdateBuffer.");
    }
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        throw std::runtime_error("ResourceManager requires a valid RenderSystem.");
    }

    // 1. Ensure upload buffer exists and is large enough
    if (!uploadBuffer || uploadBuffer->GetDesc().Width < dataSize) {
        uploadBuffer = CreateUploadBuffer(dataSize); // Create or recreate if too small
        if (!uploadBuffer) {
            throw std::runtime_error("Failed to create upload buffer for UpdateBuffer.");
        }
        uploadBuffer->SetName(L"UpdateBuffer_Intermediate");
    }

    // 2. Map the upload buffer and copy data
    void* mappedData = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr = uploadBuffer->Map(0, &readRange, &mappedData);
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to map upload buffer in UpdateBuffer.");
    }
    memcpy(mappedData, data, dataSize);
    uploadBuffer->Unmap(0, nullptr); // Indicate entire buffer might have been written

    // 3. Transition destination buffer to COPY_DEST state
    D3D12_RESOURCE_STATES stateBefore = GetResourceState(destinationBuffer);
    if (stateBefore != D3D12_RESOURCE_STATE_COPY_DEST) {
        TransitionResource(commandList, destinationBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
    }

    // 4. Record the copy command
    commandList->CopyBufferRegion(
        destinationBuffer, destinationOffset, // Destination
        uploadBuffer.Get(), 0, // Source
        dataSize);

    // 5. Transition destination buffer back to its target state
    if (stateAfterCopy != D3D12_RESOURCE_STATE_COPY_DEST) {
        TransitionResource(commandList, destinationBuffer, stateAfterCopy);
    }
    // Update tracked state immediately (barrier is recorded, will execute later)
    SetResourceState(destinationBuffer, stateAfterCopy);

    // Note: The caller is responsible for ensuring the command list is executed
    // and that the upload buffer is kept alive until the GPU finishes the copy.
    // A fence mechanism is typically used for this.
}


ComPtr<ID3D12Resource> ResourceManager::CreateUploadBuffer(UINT64 size) {
    return CreateBuffer(size, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
}

ComPtr<ID3D12Resource> ResourceManager::CreateConstantBuffer(UINT size) {
    // Constant buffers must be 256-byte aligned.
    UINT64 alignedSize = (size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1) & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);
    // Constant buffers are best in UPLOAD heap for frequent CPU updates.
    ComPtr<ID3D12Resource> cb = CreateBuffer(alignedSize, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);
    if (cb) cb->SetName((L"ConstantBuffer_" + std::to_wstring(alignedSize)).c_str());
    return cb;
}


// --- View Creation ---

ResourceDescriptor ResourceManager::CreateShaderResourceView(
    ID3D12Resource* resource, DXGI_FORMAT format,
    D3D12_SRV_DIMENSION viewDimension, UINT mostDetailedMip, UINT mipLevels) {

    if (!resource) throw std::invalid_argument("Cannot create SRV for null resource.");
    ID3D12Device* device = m_renderSystem->GetDevice();

    ResourceDescriptor desc = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (desc.heapIndex == UINT_MAX) throw std::runtime_error("Failed to allocate SRV descriptor.");

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (format == DXGI_FORMAT_UNKNOWN) {
        format = resourceDesc.Format;
    }
    // Handle typeless formats - SRV needs a typed format
    if (format == DXGI_FORMAT_R32G32B32_TYPELESS) format = DXGI_FORMAT_R32G32B32_FLOAT;
    else if (format == DXGI_FORMAT_R32G8X24_TYPELESS) format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS; // SRV for depth
    else if (format == DXGI_FORMAT_R32_TYPELESS) format = DXGI_FORMAT_R32_FLOAT;
    else if (format == DXGI_FORMAT_R24G8_TYPELESS) format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // SRV for depth
    else if (format == DXGI_FORMAT_R16_TYPELESS) format = DXGI_FORMAT_R16_UNORM;
    // Add other typeless conversions as needed

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (viewDimension == D3D12_SRV_DIMENSION_UNKNOWN) {
        // Deduce dimension from resource type
        switch (resourceDesc.Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:    viewDimension = D3D12_SRV_DIMENSION_BUFFER; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: viewDimension = D3D12_SRV_DIMENSION_TEXTURE1D; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D: viewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: viewDimension = D3D12_SRV_DIMENSION_TEXTURE3D; break;
        default: throw std::runtime_error("Unsupported resource dimension for SRV.");
        }
    }
    srvDesc.ViewDimension = viewDimension;

    if (mipLevels == static_cast<UINT>(-1)) {
        mipLevels = resourceDesc.MipLevels - mostDetailedMip;
    }

    switch (viewDimension) {
    case D3D12_SRV_DIMENSION_BUFFER:
        // Assuming raw buffer view - adjust if structured buffer needed
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = (UINT)(resourceDesc.Width / 4); // Example: Assuming 4 bytes/element
        srvDesc.Buffer.StructureByteStride = 0; // Raw buffer
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        // Check if format is DXGI_FORMAT_UNKNOWN for raw buffer view
        if (srvDesc.Format == DXGI_FORMAT_UNKNOWN) {
            srvDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Required for raw view
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            srvDesc.Buffer.NumElements = (UINT)(resourceDesc.Width / 4);
        }
        else {
            // Calculate NumElements based on format size
            // TODO: Implement proper format size calculation
            srvDesc.Buffer.NumElements = (UINT)(resourceDesc.Width); // Placeholder
        }
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        srvDesc.Texture2D.MostDetailedMip = mostDetailedMip;
        srvDesc.Texture2D.MipLevels = mipLevels;
        srvDesc.Texture2D.PlaneSlice = 0; // Assuming single plane
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        break;
        // Add cases for TEXTURE1D, TEXTURE3D, CUBE, ARRAYs etc.
    default: throw std::runtime_error("Unsupported view dimension for SRV.");
    }


    device->CreateShaderResourceView(resource, &srvDesc, desc.cpuHandle);
    return desc;
}

ResourceDescriptor ResourceManager::CreateRenderTargetView(ID3D12Resource* resource, DXGI_FORMAT format) {
    if (!resource) throw std::invalid_argument("Cannot create RTV for null resource.");
    ID3D12Device* device = m_renderSystem->GetDevice();

    ResourceDescriptor desc = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    if (desc.heapIndex == UINT_MAX) throw std::runtime_error("Failed to allocate RTV descriptor.");

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (format == DXGI_FORMAT_UNKNOWN) {
        format = resourceDesc.Format;
    }

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = format;
    // Deduce dimension - Assuming Texture2D for simplicity
    if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;
    }
    else if (resourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_BUFFER;
        // Setup Buffer RTV if needed
    }
    else {
        throw std::runtime_error("Unsupported resource dimension for RTV.");
    }


    device->CreateRenderTargetView(resource, &rtvDesc, desc.cpuHandle);
    return desc;
}

ResourceDescriptor ResourceManager::CreateDepthStencilView(ID3D12Resource* resource, DXGI_FORMAT format) {
    if (!resource) throw std::invalid_argument("Cannot create DSV for null resource.");
    ID3D12Device* device = m_renderSystem->GetDevice();

    ResourceDescriptor desc = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    if (desc.heapIndex == UINT_MAX) throw std::runtime_error("Failed to allocate DSV descriptor.");

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (format == DXGI_FORMAT_UNKNOWN) {
        format = resourceDesc.Format;
    }
    // Handle typeless depth formats
    if (format == DXGI_FORMAT_R32G8X24_TYPELESS) format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    else if (format == DXGI_FORMAT_R32_TYPELESS) format = DXGI_FORMAT_D32_FLOAT;
    else if (format == DXGI_FORMAT_R24G8_TYPELESS) format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    else if (format == DXGI_FORMAT_R16_TYPELESS) format = DXGI_FORMAT_D16_UNORM;


    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    // Assuming Texture2D
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE; // Adjust if read-only depth/stencil needed


    device->CreateDepthStencilView(resource, &dsvDesc, desc.cpuHandle);
    return desc;
}

ResourceDescriptor ResourceManager::CreateUnorderedAccessView(
    ID3D12Resource* resource, DXGI_FORMAT format,
    D3D12_UAV_DIMENSION viewDimension, ID3D12Resource* counterResource) {

    if (!resource) throw std::invalid_argument("Cannot create UAV for null resource.");
    ID3D12Device* device = m_renderSystem->GetDevice();

    ResourceDescriptor desc = AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    if (desc.heapIndex == UINT_MAX) throw std::runtime_error("Failed to allocate UAV descriptor.");

    D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
    if (format == DXGI_FORMAT_UNKNOWN) {
        format = resourceDesc.Format;
        // Handle typeless formats if necessary, similar to SRV
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = format;

    if (viewDimension == D3D12_UAV_DIMENSION_UNKNOWN) {
        // Deduce dimension
        switch (resourceDesc.Dimension) {
        case D3D12_RESOURCE_DIMENSION_BUFFER:    viewDimension = D3D12_UAV_DIMENSION_BUFFER; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D: viewDimension = D3D12_UAV_DIMENSION_TEXTURE1D; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D: viewDimension = D3D12_UAV_DIMENSION_TEXTURE2D; break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D: viewDimension = D3D12_UAV_DIMENSION_TEXTURE3D; break;
        default: throw std::runtime_error("Unsupported resource dimension for UAV.");
        }
    }
    uavDesc.ViewDimension = viewDimension;

    switch (viewDimension) {
    case D3D12_UAV_DIMENSION_BUFFER:
        // Setup buffer UAV - assuming raw for simplicity
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = (UINT)(resourceDesc.Width / 4); // Example: 4 bytes/element
        uavDesc.Buffer.StructureByteStride = 0; // Raw buffer
        uavDesc.Buffer.CounterOffsetInBytes = 0;
        uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        // Check if format is DXGI_FORMAT_UNKNOWN for raw buffer view
        if (uavDesc.Format == DXGI_FORMAT_UNKNOWN) {
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Required for raw view
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            uavDesc.Buffer.NumElements = (UINT)(resourceDesc.Width / 4);
        }
        else {
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            // TODO: Calculate NumElements based on format size
            uavDesc.Buffer.NumElements = (UINT)(resourceDesc.Width); // Placeholder
        }
        break;
    case D3D12_UAV_DIMENSION_TEXTURE2D:
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;
        break;
        // Add other dimensions
    default: throw std::runtime_error("Unsupported view dimension for UAV.");
    }

    device->CreateUnorderedAccessView(resource, counterResource, &uavDesc, desc.cpuHandle);
    return desc;
}


// --- Resource Tracking & Cleanup ---

void ResourceManager::TrackResourceInternal(ID3D12Resource* resource, ResourceType type, size_t size,
    D3D12_RESOURCE_STATES initialState) {
    if (!resource) return;

    std::lock_guard<std::mutex> lock(m_mutex);

    // Add/Update usage info
    ResourceUsage usage;
    usage.lastUsed = std::chrono::steady_clock::now();
    usage.size = size;
    usage.isPinned = false; // Default to not pinned
    usage.type = type;
    m_resourceUsage[resource] = usage;

    // Add/Update state tracking
    ResourceState state;
    state.currentState = initialState;
    m_resourceStates[resource] = state;

    // Update memory statistics
    m_memoryUsageByType[type] += size;

    // Check cache pressure - simplified, real cleanup needs more thought
    // TrimCacheIfNeeded();
}

void ResourceManager::TrackExplicitResource(const std::string& id, ID3D12Resource* resource, ResourceType type, size_t size,
    D3D12_RESOURCE_STATES initialState) {
    if (!resource || id.empty()) return;

    TrackResourceInternal(resource, type, size, initialState);

    // Add to named resources map
    std::lock_guard<std::mutex> lock(m_mutex);
    m_namedResources[id] = resource;
}


void ResourceManager::ReleaseResource(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto namedIt = m_namedResources.find(id);
    if (namedIt != m_namedResources.end()) {
        ID3D12Resource* resource = namedIt->second;
        m_namedResources.erase(namedIt); // Remove from named map

        // Remove from usage and state maps using the pointer
        auto usageIt = m_resourceUsage.find(resource);
        if (usageIt != m_resourceUsage.end()) {
            m_memoryUsageByType[usageIt->second.type] -= usageIt->second.size;
            m_resourceUsage.erase(usageIt);
        }
        m_resourceStates.erase(resource);
    }
    // Note: This only removes tracking. The ComPtr holding the resource needs to be released elsewhere.
}

void ResourceManager::ReleaseResource(ID3D12Resource* resource) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove from usage and state maps
    auto usageIt = m_resourceUsage.find(resource);
    if (usageIt != m_resourceUsage.end()) {
        m_memoryUsageByType[usageIt->second.type] -= usageIt->second.size;
        m_resourceUsage.erase(usageIt);
    }
    m_resourceStates.erase(resource);

    // Remove from named map if it exists there
    for (auto it = m_namedResources.begin(); it != m_namedResources.end(); ) {
        if (it->second == resource) {
            it = m_namedResources.erase(it);
        }
        else {
            ++it;
        }
    }
}


void ResourceManager::ReleaseUnusedResources(std::chrono::seconds maxAge) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_renderSystem) return;

    // Wait for GPU idle before releasing potentially used resources
    // This is a coarse approach; finer-grained fence management per resource is better
    m_renderSystem->WaitForGpu();

    auto now = std::chrono::steady_clock::now();
    std::vector<ID3D12Resource*> toRemove;

    for (auto const& [resource, usage] : m_resourceUsage) {
        if (!usage.isPinned) {
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - usage.lastUsed);
            if (age > maxAge) {
                toRemove.push_back(resource);
            }
        }
    }

    for (ID3D12Resource* resource : toRemove) {
        OutputDebugStringA(("Releasing unused resource: " + PtrToID(resource) + "\n").c_str());
        // This removes tracking. The actual resource release happens
        // when the last ComPtr pointing to it goes out of scope.
        // If this manager *owned* the ComPtrs, we would release them here.
        ReleaseResource(resource);
    }
}


// --- Resource State Management ---

D3D12_RESOURCE_STATES ResourceManager::GetResourceState(ID3D12Resource* resource) const {
    if (!resource) return D3D12_RESOURCE_STATE_COMMON; // Or throw
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resourceStates.find(resource);
    if (it != m_resourceStates.end()) {
        return it->second.currentState;
    }
    // If not tracked, assume common state. This might be dangerous.
    // Consider throwing or logging a warning if a resource state is queried but not tracked.
    OutputDebugStringA(("Warning: Queried state of untracked resource: " + PtrToID(resource) + "\n").c_str());
    return D3D12_RESOURCE_STATE_COMMON;
}

// Set state directly - Use primarily internally or when absolutely sure.
void ResourceManager::SetResourceState(ID3D12Resource* resource, D3D12_RESOURCE_STATES state) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_resourceStates[resource].currentState = state;
}

D3D12_RESOURCE_BARRIER ResourceManager::TransitionBarrier(ID3D12Resource* resource,
    D3D12_RESOURCE_STATES stateBefore,
    D3D12_RESOURCE_STATES stateAfter,
    UINT subresource) {
    return {
        D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
        D3D12_RESOURCE_BARRIER_FLAG_NONE,
        { resource, subresource, stateBefore, stateAfter }
    };
}


void ResourceManager::TransitionResource(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES newState) {
    if (!commandList || !resource) return;

    D3D12_RESOURCE_STATES currentState = GetResourceState(resource);

    if (currentState != newState) {
        D3D12_RESOURCE_BARRIER barrier = TransitionBarrier(resource, currentState, newState);
        commandList->ResourceBarrier(1, &barrier);
        SetResourceState(resource, newState); // Update tracked state
    }
}

void ResourceManager::TransitionResources(ID3D12GraphicsCommandList* commandList,
    const std::vector<D3D12_RESOURCE_BARRIER>& barriers) {
    if (!commandList || barriers.empty()) return;

    commandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    // Update tracked state for each transitioned resource
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& barrier : barriers) {
        if (barrier.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION) {
            m_resourceStates[barrier.Transition.pResource].currentState = barrier.Transition.StateAfter;
        }
    }
}


// --- Resource Usage & Pooling ---

void ResourceManager::NotifyResourceUsed(ID3D12Resource* resource) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resourceUsage.find(resource);
    if (it != m_resourceUsage.end()) {
        it->second.lastUsed = std::chrono::steady_clock::now();
    }
}

void ResourceManager::PinResource(ID3D12Resource* resource, bool pin) {
    if (!resource) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resourceUsage.find(resource);
    if (it != m_resourceUsage.end()) {
        it->second.isPinned = pin;
    }
    else {
        OutputDebugStringA(("Warning: Tried to pin untracked resource: " + PtrToID(resource) + "\n").c_str());
    }
}

bool ResourceManager::IsPinned(ID3D12Resource* resource) const {
    if (!resource) return false;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_resourceUsage.find(resource);
    if (it != m_resourceUsage.end()) {
        return it->second.isPinned;
    }
    return false; // Untracked resources are not pinned
}


// --- Descriptor Management ---

ResourceDescriptor ResourceManager::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE type) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto poolIt = m_descriptorPools.find(type);
    if (poolIt == m_descriptorPools.end()) {
        throw std::runtime_error("Descriptor pool for type " + std::to_string(type) + " not initialized.");
    }
    auto& pool = poolIt->second;

    // Simple linear search for a free slot
    for (UINT i = 0; i < pool.capacity; ++i) {
        if (!pool.allocated[i]) {
            pool.allocated[i] = true;
            pool.size++;

            ResourceDescriptor desc;
            desc.heapIndex = i;
            desc.type = type;
            desc.cpuHandle = pool.cpuStart;
            desc.cpuHandle.ptr += static_cast<SIZE_T>(i) * pool.descriptorSize; // Correct offset calculation

            if (pool.heap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
                desc.gpuHandle = pool.gpuStart;
                desc.gpuHandle.ptr += static_cast<SIZE_T>(i) * pool.descriptorSize; // Correct offset calculation
            }
            else {
                desc.gpuHandle = {}; // Invalid GPU handle
            }

            return desc;
        }
    }

    // Out of descriptors - resize or throw
    throw std::runtime_error("Out of descriptors in pool type " + std::to_string(type));
    // return { UINT_MAX, {}, {}, type }; // Indicate failure
}

void ResourceManager::FreeDescriptor(const ResourceDescriptor& descriptor) {
    if (descriptor.heapIndex == UINT_MAX) return; // Invalid descriptor

    std::lock_guard<std::mutex> lock(m_mutex);

    auto poolIt = m_descriptorPools.find(descriptor.type);
    if (poolIt == m_descriptorPools.end()) {
        OutputDebugStringA(("Warning: Trying to free descriptor from non-existent pool type " + std::to_string(descriptor.type) + "\n").c_str());
        return;
    }
    auto& pool = poolIt->second;

    if (descriptor.heapIndex < pool.capacity && pool.allocated[descriptor.heapIndex]) {
        pool.allocated[descriptor.heapIndex] = false;
        pool.size--;
    }
    else {
        OutputDebugStringA(("Warning: Trying to free invalid or already freed descriptor index " + std::to_string(descriptor.heapIndex) + " in pool type " + std::to_string(descriptor.type) + "\n").c_str());
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::GetCpuDescriptorHandle(const ResourceDescriptor& descriptor) const {
    if (descriptor.heapIndex == UINT_MAX) return {};
    // Recalculate based on stored start and index - safer than storing potentially stale handle directly
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& pool = m_descriptorPools.at(descriptor.type);
    D3D12_CPU_DESCRIPTOR_HANDLE handle = pool.cpuStart;
    handle.ptr += static_cast<SIZE_T>(descriptor.heapIndex) * pool.descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::GetGpuDescriptorHandle(const ResourceDescriptor& descriptor) const {
    if (descriptor.heapIndex == UINT_MAX) return {};
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto& pool = m_descriptorPools.at(descriptor.type);
    if (!(pool.heap->GetDesc().Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)) return {}; // Not shader visible

    D3D12_GPU_DESCRIPTOR_HANDLE handle = pool.gpuStart;
    handle.ptr += static_cast<SIZE_T>(descriptor.heapIndex) * pool.descriptorSize;
    return handle;
}

UINT ResourceManager::GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto poolIt = m_descriptorPools.find(type);
    if (poolIt != m_descriptorPools.end()) {
        return poolIt->second.descriptorSize;
    }
    // Fallback to querying device if pool not found (shouldn't happen after init)
    return m_renderSystem ? m_renderSystem->GetDevice()->GetDescriptorHandleIncrementSize(type) : 0;
}


// --- Memory Statistics ---

size_t ResourceManager::GetTotalMemoryUsage() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t total = 0;
    for (const auto& pair : m_memoryUsageByType) {
        total += pair.second;
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

// --- Cache Management ---

void ResourceManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Clear tracking maps - does NOT release the actual ID3D12Resource objects
    m_resourceUsage.clear();
    m_resourceStates.clear();
    m_namedResources.clear();
    m_memoryUsageByType.clear();

    // Reset descriptor pools allocation status (doesn't delete heaps)
    for (auto& pair : m_descriptorPools) {
        auto& pool = pair.second;
        std::fill(pool.allocated.begin(), pool.allocated.end(), false);
        pool.size = 0;
    }
    OutputDebugStringA("ResourceManager cache cleared.\n");
}

void ResourceManager::SetCacheLimit(size_t maxMemorySizeBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxCacheSize = maxMemorySizeBytes;
    // Optional: Immediately trigger ReleaseUnusedResources if over limit
    // ReleaseUnusedResources(...)
}

// --- Private Helpers ---

std::string ResourceManager::GetResourceID(ID3D12Resource* resource) const {
    if (!resource) return "";
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& pair : m_namedResources) {
        if (pair.second == resource) {
            return pair.first;
        }
    }
    // Fallback to pointer string if not found by name
    return PtrToID(resource);
}

