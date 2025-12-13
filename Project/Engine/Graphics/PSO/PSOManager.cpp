#include "PSOManager.h"

#include "Shader/ShaderCompiler.h"
#include "Logger/Logger.h"

#include <directx/d3dx12.h>
#include <cassert>

using Microsoft::WRL::ComPtr;

PSOManager::PSOManager(ID3D12Device *device, ShaderCompiler *shaderCompiler) {
    assert(device);
    assert(shaderCompiler);

    device_ = device;
    shaderCompiler_ = shaderCompiler;
}

void PSOManager::Initialize() {
    LOG_INFO("PSOManager: Initialize");

    CreateSpritePipeline();
}

// ======================================================
// Sprite Pipeline
// ======================================================
void PSOManager::CreateSpritePipeline() {
    LOG_INFO("PSOManager: CreateSpritePipeline");

    // ------------------------------
    // RootSignature
    // ------------------------------
    D3D12_ROOT_PARAMETER params[2]{};

    // b0 : ConstantBuffer
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t0 : Texture SRV
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.OffsetInDescriptorsFromTableStart =
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges = &range;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Sampler
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(params);
    rsDesc.pParameters = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &sampler;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> rsBlob;
    ComPtr<ID3DBlob> rsError;

    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &rsError);

    if (FAILED(hr)) {
        LOG_ERROR("PSOManager: RootSignature serialize failed");
        if (rsError) {
            OutputDebugStringA(
                static_cast<char *>(rsError->GetBufferPointer()));
        }
        assert(false);
    }

    hr = device_->CreateRootSignature(0, rsBlob->GetBufferPointer(),
                                      rsBlob->GetBufferSize(),
                                      IID_PPV_ARGS(&spriteRoot_));

    if (FAILED(hr)) {
        LOG_ERROR("PSOManager: CreateRootSignature failed");
        assert(false);
    }

    // ------------------------------
    // Shader
    // ------------------------------
    auto vs = shaderCompiler_->CompileShader("Sprite/Sprite.VS.hlsl", "main",
                                             "vs_6_0");
    auto ps = shaderCompiler_->CompileShader("Sprite/Sprite.PS.hlsl", "main",
                                             "ps_6_0");

    // ------------------------------
    // InputLayout
    // ------------------------------
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // ------------------------------
    // PSO
    // ------------------------------
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = spriteRoot_.Get();
    pso.InputLayout = {layout, _countof(layout)};
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};

    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = UINT_MAX;

    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

    // Alpha Blend
    auto &rt = pso.BlendState.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D12_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D12_BLEND_ONE;
    rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // No depth / no cull
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.StencilEnable = FALSE;

    hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&spritePSO_));

    if (FAILED(hr)) {
        LOG_ERROR("PSOManager: CreateGraphicsPipelineState failed");
        assert(false);
    }
}
