#include "SpriteCommon.h"
#include "ShaderCompiler.h"
#include <cassert>
#include <wrl.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

void SpriteCommon::Initialize(ID3D12Device *device) {
    assert(device);
    CreateRootSignature(device);
}

void SpriteCommon::CreateGraphicsPipeline(
    ShaderCompiler &compiler,
    ID3D12Device *device,
    const std::wstring &vsPath,
    const std::wstring &psPath,
    const PipelineFormats &formats,
    const std::wstring &vsEntry,
    const std::wstring &psEntry) {
    assert(device && rootSignature_);

    auto vsRes = compiler.CompileFromFile(vsPath, vsEntry, L"vs_6_0");
    auto psRes = compiler.CompileFromFile(psPath, psEntry, L"ps_6_0");
    if (!vsRes.succeeded) {
        if (vsRes.errors) OutputDebugStringA(vsRes.errors->GetStringPointer());
        assert(false); return;
    }
    if (!psRes.succeeded) {
        if (psRes.errors) OutputDebugStringA(psRes.errors->GetStringPointer());
        assert(false); return;
    }

    // 入力レイアウト：
    // - POSITION は HLSL 側が float3。頂点メモリ側は float4 だが W は捨てる。
    // - TEXCOORD は頂点構造体中の16B目から（position(Vector4)ぶんスキップ）
    D3D12_INPUT_ELEMENT_DESC elems[2] = {};
    elems[0].SemanticName = "POSITION";
    elems[0].Format = DXGI_FORMAT_R32G32B32_FLOAT; // float3
    elems[0].InputSlot = 0;
    elems[0].AlignedByteOffset = 0; // 先頭 (x,y,z) を読む。w は無視される。
    elems[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    elems[1].SemanticName = "TEXCOORD";
    elems[1].SemanticIndex = 0;
    elems[1].Format = DXGI_FORMAT_R32G32_FLOAT; // float2
    elems[1].InputSlot = 0;
    elems[1].AlignedByteOffset = 16; // Vector4 position 分をスキップしてUV
    elems[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

    D3D12_INPUT_LAYOUT_DESC il{};
    il.pInputElementDescs = elems;
    il.NumElements = _countof(elems);

    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rast{};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_NONE; // 2Dなので両面OFF（裏面カリング無）

    D3D12_DEPTH_STENCIL_DESC ds{};
    if (formats.dsvFormat == DXGI_FORMAT_UNKNOWN) {
        ds.DepthEnable = FALSE;
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    } else {
        ds.DepthEnable = TRUE;
        ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSignature_.Get();
    pso.InputLayout = il;
    pso.VS = {vsRes.object->GetBufferPointer(), vsRes.object->GetBufferSize()};
    pso.PS = {psRes.object->GetBufferPointer(), psRes.object->GetBufferSize()};
    pso.BlendState = blend;
    pso.RasterizerState = rast;
    pso.DepthStencilState = ds;
    pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = formats.numRenderTargets;
    for (UINT i = 0; i < formats.numRenderTargets; ++i) {
        pso.RTVFormats[i] = formats.rtvFormat;
    }
    pso.DSVFormat = formats.dsvFormat;
    pso.SampleDesc.Count = 1;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &pso, IID_PPV_ARGS(pipelineState_.ReleaseAndGetAddressOf()));
    assert(SUCCEEDED(hr));
}

void SpriteCommon::ApplyCommonDrawSettings(
    ID3D12GraphicsCommandList *cmd,
    D3D12_PRIMITIVE_TOPOLOGY topology) const {
    assert(cmd && rootSignature_ && pipelineState_);
    cmd->SetGraphicsRootSignature(rootSignature_.Get());
    cmd->SetPipelineState(pipelineState_.Get());
    cmd->IASetPrimitiveTopology(topology);
}

void SpriteCommon::CreateRootSignature(ID3D12Device *device) {
    // 今回は CBV だけ（p0: PS b0 / p1: VS b1）。SRV/Sampler は不要。
    D3D12_ROOT_PARAMETER params[2]{};

    // p0: PS CBV(b0) Material
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[0].Descriptor.ShaderRegister = 0; // b0

    // p1: VS CBV(b1) Transform
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    params[1].Descriptor.ShaderRegister = 1; // b1

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
        | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> sig, err;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &err);
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((char *)err->GetBufferPointer());
        assert(false);
    }
    hr = device->CreateRootSignature(
        0, sig->GetBufferPointer(), sig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.ReleaseAndGetAddressOf()));
    assert(SUCCEEDED(hr));
}
