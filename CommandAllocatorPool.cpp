// GameOverlay - CommandAllocatorPool.cpp
// Phase 6: DirectX 12 Migration
// Manages a pool of command allocators for efficient command list recycling

#include "CommandAllocatorPool.h"
#include <stdexcept>

CommandAllocatorPool::CommandAllocatorPool(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type)
    : m_device(device), m_type(type) {

    // Pre-allocate some command allocators
    for (int i = 0; i < 3; i++) {
        ComPtr<ID3D12CommandAllocator> allocator;
        HRESULT hr = m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&allocator));
        if (SUCCEEDED(hr)) {
            CommandAllocatorEntry entry;
            entry.allocator = allocator;
            entry.fenceValue = 0;

            m_allocatorPool.push_back(entry);
            m_availableAllocators.push(m_allocatorPool.size() - 1);
        }
    }
}

CommandAllocatorPool::~CommandAllocatorPool() {
    Clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::GetCommandAllocator(uint64_t completedFenceValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // First check if there's an available allocator in the queue
    if (!m_availableAllocators.empty()) {
        size_t allocatorIndex = m_availableAllocators.front();
        m_availableAllocators.pop();

        CommandAllocatorEntry& entry = m_allocatorPool[allocatorIndex];

        // Reset the allocator if it's safe (GPU has finished using it)
        if (entry.fenceValue <= completedFenceValue) {
            entry.allocator->Reset();
            return entry.allocator.Get();
        }
    }

    // If we get here, either there are no available allocators or none are ready to be reset
    // Check all allocators to see if any are ready but not in the queue
    for (CommandAllocatorEntry& entry : m_allocatorPool) {
        if (entry.fenceValue <= completedFenceValue) {
            entry.allocator->Reset();
            return entry.allocator.Get();
        }
    }

    // If we still don't have an allocator, create a new one
    return CreateCommandAllocator();
}

void CommandAllocatorPool::ReleaseCommandAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Find the allocator in the pool and update its fence value
    for (size_t i = 0; i < m_allocatorPool.size(); i++) {
        if (m_allocatorPool[i].allocator.Get() == allocator) {
            m_allocatorPool[i].fenceValue = fenceValue;
            m_availableAllocators.push(i);
            return;
        }
    }

    // If the allocator wasn't found, it might be a new one that was created on-demand
    // Add it to the pool
    CommandAllocatorEntry entry;
    entry.allocator = allocator;
    entry.fenceValue = fenceValue;

    m_allocatorPool.push_back(entry);
    m_availableAllocators.push(m_allocatorPool.size() - 1);
}

void CommandAllocatorPool::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_allocatorPool.clear();

    // Clear the queue
    while (!m_availableAllocators.empty()) {
        m_availableAllocators.pop();
    }
}

ID3D12CommandAllocator* CommandAllocatorPool::CreateCommandAllocator() {
    ComPtr<ID3D12CommandAllocator> allocator;
    HRESULT hr = m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&allocator));
    if (FAILED(hr)) {
        throw std::runtime_error("Failed to create command allocator");
    }

    CommandAllocatorEntry entry;
    entry.allocator = allocator;
    entry.fenceValue = 0;

    m_allocatorPool.push_back(entry);

    return allocator.Get();
}
