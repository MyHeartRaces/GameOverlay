// GameOverlay - PipelineStateManager.cpp
// Phase 6: DirectX 12 Migration
// Manages pipeline state objects and root signatures for DirectX 12

#include "PipelineStateManager.h"
#include "RenderSystem.h"
#include <d3dcompiler.h>
#include <stdexcept>

#pragma comment(lib, "d3dcompiler.lib")

// Vertex shader for basic rendering
const char* g_BasicVertexShader = R"(
struct VSInput
{
    float3 position : POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0f);
    output.texCoord = input.texCoord;
    output.color = input.color;
    return output;
}
)";

// Pixel shader for basic rendering
const char* g_BasicPixelShader = R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

// Pixel shader for textured rendering
const char* g_TexturePixelShader = R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float4 color : COLOR;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.texCoord) * input.color;
}
)";

PipelineStateManager::PipelineStateManager(RenderSystem* renderSystem)
    : m_renderSystem(renderSystem) {
}

PipelineStateManager::~PipelineStateManager() {
    ClearCache();
}

void PipelineStateManager::Initialize() {
    // Create default root signatures
    m_defaultRootSignature = CreateDefaultRootSignature();
    m_textureRootSignature = CreateTextureRootSignature();

    // Pre-create some common pipeline states
    PipelineStateKey defaultKey;
    GetPipelineState(defaultKey);
}

ID3D12PipelineState* PipelineStateManager::GetPipelineState(const PipelineStateKey& key) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if pipeline state already exists
    auto it = m_pipelineStates.find(key);
    if (it != m_pipelineStates.end()) {
        return it->second.Get();
    }

    // Create new pipeline state
    ComPtr<ID3D12PipelineState> pipelineState = CreatePipelineState(key);
    if (!pipelineState) {
        return nullptr;
    }

    // Cache and return
    m_pipelineStates[key] = pipelineState;
    return pipelineState.Get();
}

ID3D12RootSignature* PipelineStateManager::GetDefaultRootSignature() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_defaultRootSignature) {
        m_defaultRootSignature = CreateDefaultRootSignature();
    }

    return m_defaultRootSignature.Get();
}

ID3D12RootSignature* PipelineStateManager::GetTextureRootSignature() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_textureRootSignature) {
        m_textureRootSignature = CreateTextureRootSignature();
    }

    return m_textureRootSignature.Get();
}

void PipelineStateManager::ClearCache() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_pipelineStates.clear();
    m_defaultRootSignature.Reset();
    m_textureRootSignature.Reset();
}

ComPtr<ID3D12PipelineState> PipelineStateManager::CreatePipelineState(const PipelineStateKey& key) {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    // Compile shaders
    ComPtr<ID3DBlob> vertexShaderBlob;
    ComPtr<ID3DBlob> pixelShaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    HRESULT hr;

    // Vertex shader
    hr = D3DCompile(
        g_BasicVertexShader, strlen(g_BasicVertexShader),
        nullptr, nullptr, nullptr, "main",
        ("vs_" + key.shaderModel).c_str(),
        compileFlags, 0, &vertexShaderBlob, &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return nullptr;
    }

    // Pixel shader - choose based on blending mode
    const char* pixelShaderSource = (key.blendMode == PipelineStateKey::NoBlend) ?
        g_BasicPixelShader : g_TexturePixelShader;

    hr = D3DCompile(
        pixelShaderSource, strlen(pixelShaderSource),
        nullptr, nullptr, nullptr, "main",
        ("ps_" + key.shaderModel).c_str(),
        compileFlags, 0, &pixelShaderBlob, &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return nullptr;
    }

    // Define the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Create the root signature to use
    ID3D12RootSignature* rootSignature = (key.blendMode == PipelineStateKey::NoBlend) ?
        GetDefaultRootSignature() : GetTextureRootSignature();

    if (!rootSignature) {
        return nullptr;
    }

    // Create pipeline state
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
    psoDesc.RasterizerState = CreateRasterizerDesc(key.rasterizerMode);
    psoDesc.BlendState = CreateBlendDesc(key.blendMode);
    psoDesc.DepthStencilState = CreateDepthStencilDesc(key.depthMode);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = key.renderTargetFormat;
    psoDesc.DSVFormat = key.depthStencilFormat;
    psoDesc.SampleDesc.Count = 1;

    ComPtr<ID3D12PipelineState> pipelineState;
    hr = m_renderSystem->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr)) {
        return nullptr;
    }

    return pipelineState;
}

ComPtr<ID3D12RootSignature> PipelineStateManager::CreateDefaultRootSignature() {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    // Create a root signature with no parameters
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.NumParameters = 0;
    rootSignatureDesc.pParameters = nullptr;
    rootSignatureDesc.NumStaticSamplers = 0;
    rootSignatureDesc.pStaticSamplers = nullptr;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize the root signature
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        return nullptr;
    }

    // Create the root signature
    ComPtr<ID3D12RootSignature> rootSignature;
    hr = m_renderSystem->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        return nullptr;
    }

    return rootSignature;
}

ComPtr<ID3D12RootSignature> PipelineStateManager::CreateTextureRootSignature() {
    if (!m_renderSystem || !m_renderSystem->GetDevice()) {
        return nullptr;
    }

    // Create a root signature with texture and sampler
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);

    // Create the root signature
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
    rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
    rootSignatureDesc.Desc_1_1.NumStaticSamplers = 0;
    rootSignatureDesc.Desc_1_1.pStaticSamplers = nullptr;
    rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize the root signature
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            OutputDebugStringA(static_cast<const char*>(error->GetBufferPointer()));
        }
        return nullptr;
    }

    // Create the root signature
    ComPtr<ID3D12RootSignature> rootSignature;
    hr = m_renderSystem->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        return nullptr;
    }

    return rootSignature;
}

D3D12_BLEND_DESC PipelineStateManager::CreateBlendDesc(PipelineStateKey::BlendMode blendMode) {
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;

    switch (blendMode) {
    case PipelineStateKey::NoBlend:
        blendDesc.RenderTarget[0].BlendEnable = FALSE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;

    case PipelineStateKey::AlphaBlend:
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;

    case PipelineStateKey::AddBlend:
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;

    case PipelineStateKey::SubtractBlend:
        blendDesc.RenderTarget[0].BlendEnable = TRUE;
        blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
        blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_REV_SUBTRACT;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        break;
    }

    return blendDesc;
}

D3D12_RASTERIZER_DESC PipelineStateManager::CreateRasterizerDesc(PipelineStateKey::RasterizerMode rasterizerMode) {
    D3D12_RASTERIZER_DESC rasterizerDesc = {};

    switch (rasterizerMode) {
    case PipelineStateKey::Solid:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        break;

    case PipelineStateKey::Wireframe:
        rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
        break;
    }

    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return rasterizerDesc;
}

D3D12_DEPTH_STENCIL_DESC PipelineStateManager::CreateDepthStencilDesc(PipelineStateKey::DepthMode depthMode) {
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};

    switch (depthMode) {
    case PipelineStateKey::NoDepth:
        depthStencilDesc.DepthEnable = FALSE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;

    case PipelineStateKey::ReadOnly:
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;

    case PipelineStateKey::ReadWrite:
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
        break;
    }

    depthStencilDesc.StencilEnable = FALSE;
    depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

    depthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    depthStencilDesc.BackFace = depthStencilDesc.FrontFace;

    return depthStencilDesc;
}
