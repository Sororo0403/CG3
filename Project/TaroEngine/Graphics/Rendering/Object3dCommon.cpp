#include "Object3dCommon.h"
#include <cassert>
#include <vector>
#include "DirectXTex/d3dx12.h"
#include "ShaderCompiler.h"

using Microsoft::WRL::ComPtr;

static D3D12_STATIC_SAMPLER_DESC MakeStaticSampler_s0() {
    D3D12_STATIC_SAMPLER_DESC s{};
    s.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    s.MipLODBias = 0.0f;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    s.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    s.MinLOD = 0.0f;
    s.MaxLOD = D3D12_FLOAT32_MAX;
    s.ShaderRegister = 0;              // s0
    s.RegisterSpace = 0;
    s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    return s;
}

void Object3dCommon::Initialize(DirectXCommon *dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    CreateRootSignature();
    CreateGraphicsPipelineState();
}

void Object3dCommon::CreateRootSignature() {
    // Root Parameters: 0=CBV(b0), 1=CBV(b1), 2=CBV(b2), 3=SRV table (t0)
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[4]{};

    // b0 TransformCB
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0; // b0
    params[0].Descriptor.RegisterSpace = 0;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // b1 MaterialCB
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].Descriptor.ShaderRegister = 1; // b1
    params[1].Descriptor.RegisterSpace = 0;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // b2 DirLightCB
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[2].Descriptor.ShaderRegister = 2; // b2
    params[2].Descriptor.RegisterSpace = 0;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // t0 SRV (descriptor table)
    D3D12_ROOT_DESCRIPTOR_TABLE table{};
    table.NumDescriptorRanges = 1;
    table.pDescriptorRanges = &srvRange;

    params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[3].DescriptorTable = table;
    params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static sampler s0
    D3D12_STATIC_SAMPLER_DESC samp = MakeStaticSampler_s0();

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &samp;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    assert(SUCCEEDED(hr) && sigBlob);

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipelineState() {
    // === DXC で VS/PS をコンパイル ===
    ShaderCompiler sc;
    bool ok = sc.Initialize();
    assert(ok);

    std::vector<ShaderCompiler::Define> defines;   // 必要なら追加
    std::vector<std::wstring> extraArgs;           // 例: {L"-Zi", L"-Qembed_debug"} 等

#ifdef _DEBUG
    sc.SetOptimizationLevel(0);
    extraArgs = {L"-Zi", L"-Qembed_debug"};
#else
    sc.EnableDebug(false);
    sc.SetOptimizationLevel(3);
#endif

    auto vsRes = sc.CompileFromFile(
        L"Resources/Shaders/Object3dVS.hlsl",
        L"main", L"vs_6_0", defines, extraArgs);
    assert(vsRes.succeeded && vsRes.object);

    auto psRes = sc.CompileFromFile(
        L"Resources/Shaders/Object3dPS.hlsl",
        L"main", L"ps_6_0", defines, extraArgs);
    assert(psRes.succeeded && psRes.object);

    // === 入力レイアウト ===
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // === PSO ===
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = {vsRes.object->GetBufferPointer(), vsRes.object->GetBufferSize()};
    psoDesc.PS = {psRes.object->GetBufferPointer(), psRes.object->GetBufferSize()};
    psoDesc.InputLayout = {inputLayout, _countof(inputLayout)};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &psoDesc, IID_PPV_ARGS(&graphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::SetCommonDrawSetting(ID3D12GraphicsCommandList *cmdList) {
    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(graphicsPipelineState_.Get());
}
