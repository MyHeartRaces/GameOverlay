// GameOverlay - CommandAllocatorPool.h
// Phase 6: DirectX 12 Migration
// Manages a pool of command allocators for efficient command list recycling

#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <queue>
#include <mutex>

using Microsoft::WRL::ComPtr;

// Command allocator with fence information
struct CommandAllocatorEntry {
    ComPtr<ID3D12CommandAllocator> allocator;
    uint64_t fenceValue = 0;
};

// Pool of command allocators for reuse
class CommandAllocatorPool {
public:
    CommandAllocatorPool(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    ~CommandAllocatorPool();

    // Disable copy and move
    CommandAllocatorPool(const CommandAllocatorPool&) = delete;
    CommandAllocatorPool& operator=(const CommandAllocatorPool&) = delete;
    CommandAllocatorPool(CommandAllocatorPool&&) = delete;
    CommandAllocatorPool& operator=(CommandAllocatorPool&&) = delete;

    // Get a command allocator from the pool
    ID3D12CommandAllocator* GetCommandAllocator(uint64_t completedFenceValue);

    // Return a command allocator to the pool for reuse
    void ReleaseCommandAllocator(uint64_t fenceValue, ID3D12CommandAllocator* allocator);

    // Clear all allocators
    void Clear();

private:
    // Create a new command allocator
    ID3D12CommandAllocator* CreateCommandAllocator();

    // Device reference
    ID3D12Device* m_device = nullptr;

    // Command list type
    D3D12_COMMAND_LIST_TYPE m_type;

    // Pool of command allocators
    std::vector<CommandAllocatorEntry> m_allocatorPool;

    // Queue of available allocators
    std::queue<size_t> m_availableAllocators;

    // Thread safety
    std::mutex m_mutex;
};