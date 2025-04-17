// GameOverlay - PipelineStateManager.h
// Phase 6: DirectX 12 Migration
// Manages pipeline state objects and root signatures for DirectX 12

#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <mutex>

using Microsoft::WRL::ComPtr;

// Structure to identify a pipeline state configuration
struct PipelineStateKey {
    enum BlendMode {
        NoBlend,
        AlphaBlend,
        AddBlend,
        SubtractBlend,
        // Add more as needed
    };

    enum RasterizerMode {
        Solid,
        Wireframe,
        // Add more as needed
    };

    enum DepthMode {
        NoDepth,
        ReadOnly,
        ReadWrite,
        // Add more as needed
    };

    BlendMode blendMode = AlphaBlend;
    RasterizerMode rasterizerMode = Solid;
    DepthMode depthMode = NoDepth;
    DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT depthStencilFormat = DXGI_FORMAT_UNKNOWN;
    std::string shaderModel = "5_1";

    // Helper struct for hashing
    struct Hash {
        size_t operator()(const PipelineStateKey& key) const {
            size_t h1 = std::hash<int>()(static_cast<int>(key.blendMode));
            size_t h2 = std::hash<int>()(static_cast<int>(key.rasterizerMode));
            size_t h3 = std::hash<int>()(static_cast<int>(key.depthMode));
            size_t h4 = std::hash<int>()(key.renderTargetFormat);
            size_t h5 = std::hash<int>()(key.depthStencilFormat);
            size_t h6 = std::hash<std::string>()(key.shaderModel);

            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4) ^ (h6 << 5);
        }
    };

    // Equality operator for hash map
    bool operator==(const PipelineStateKey& other) const {
        return blendMode == other.blendMode &&
            rasterizerMode == other.rasterizerMode &&
            depthMode == other.depthMode &&
            renderTargetFormat == other.renderTargetFormat &&
            depthStencilFormat == other.depthStencilFormat &&
            shaderModel == other.shaderModel;
    }
};

// Forward declaration
class RenderSystem;

// Manager for pipeline state objects and root signatures
class PipelineStateManager {
public:
    PipelineStateManager(RenderSystem* renderSystem);
    ~PipelineStateManager();

    // Disable copy and move
    PipelineStateManager(const PipelineStateManager&) = delete;
    PipelineStateManager& operator=(const PipelineStateManager&) = delete;
    PipelineStateManager(PipelineStateManager&&) = delete;
    PipelineStateManager& operator=(PipelineStateManager&&) = delete;

    // Initialize the manager
    void Initialize();

    // Get or create a pipeline state for the given configuration
    ID3D12PipelineState* GetPipelineState(const PipelineStateKey& key);

    // Get or create root signatures
    ID3D12RootSignature* GetDefaultRootSignature();
    ID3D12RootSignature* GetTextureRootSignature();

    // Clear all cached pipeline states and root signatures
    void ClearCache();

private:
    // Create a pipeline state for the given configuration
    ComPtr<ID3D12PipelineState> CreatePipelineState(const PipelineStateKey& key);

    // Create root signatures
    ComPtr<ID3D12RootSignature> CreateDefaultRootSignature();
    ComPtr<ID3D12RootSignature> CreateTextureRootSignature();

    // Helper to create blend description based on blend mode
    D3D12_BLEND_DESC CreateBlendDesc(PipelineStateKey::BlendMode blendMode);

    // Helper to create rasterizer description based on rasterizer mode
    D3D12_RASTERIZER_DESC CreateRasterizerDesc(PipelineStateKey::RasterizerMode rasterizerMode);

    // Helper to create depth stencil description based on depth mode
    D3D12_DEPTH_STENCIL_DESC CreateDepthStencilDesc(PipelineStateKey::DepthMode depthMode);

    // Resource pointer (not owned)
    RenderSystem* m_renderSystem = nullptr;

    // Cache of pipeline states
    std::unordered_map<PipelineStateKey, ComPtr<ID3D12PipelineState>, PipelineStateKey::Hash> m_pipelineStates;

    // Root signatures
    ComPtr<ID3D12RootSignature> m_defaultRootSignature;
    ComPtr<ID3D12RootSignature> m_textureRootSignature;

    // Thread safety
    std::mutex m_mutex;
};