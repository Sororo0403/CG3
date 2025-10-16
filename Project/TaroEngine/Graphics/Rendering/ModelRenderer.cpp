#define NOMINMAX
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include "Mesh.h"
#include <d3d12.h>
#include <dxcapi.h>
#include <wrl.h>
#include <cassert>
#include <vector>
#include <string>

using Microsoft::WRL::ComPtr;

// 最小限の DXC ランタイムコンパイル（/Zi などは割愛）
static ComPtr<IDxcBlob> CompileHLSL(const wchar_t *path, const wchar_t *entry, const wchar_t *target) {
    ComPtr<IDxcUtils> utils;
    ComPtr<IDxcCompiler3> comp;
    ComPtr<IDxcIncludeHandler> inc;
    HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));  assert(SUCCEEDED(hr));
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&comp));        assert(SUCCEEDED(hr));
    hr = utils->CreateDefaultIncludeHandler(&inc);                         assert(SUCCEEDED(hr));

    std::vector<const wchar_t *> args = {
        path,
        L"-E", entry,
        L"-T", target,
        L"-Zpr",           // 矩形行列(row_major)を無効化 → 既定の column_major を使う
        L"-O3"
    };

    ComPtr<IDxcBlobEncoding> src;
    hr = utils->LoadFile(path, nullptr, &src); assert(SUCCEEDED(hr));

    DxcBuffer buf{};
    buf.Ptr = src->GetBufferPointer();
    buf.Size = src->GetBufferSize();
    buf.Encoding = DXC_CP_ACP;

    ComPtr<IDxcResult> result;
    hr = comp->Compile(&buf, args.data(), (UINT)args.size(), inc.Get(), IID_PPV_ARGS(&result));
    assert(SUCCEEDED(hr));

    ComPtr<IDxcBlobUtf8> errors;
    result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
    if (errors && errors->GetStringLength() > 0) {
        OutputDebugStringA(errors->GetStringPointer()); // デバッグ出力にエラー表示
    }

    ComPtr<IDxcBlob> blob;
    result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
    assert(blob);
    return blob;
}

// =====================

void ModelRenderer::Initialize(ID3D12Device *device, ID3D12DescriptorHeap * /*srvHeap*/) {
    device_.Reset();
    device_ = device;

    CreateRootSignature();
    CreatePipelineState();

    // シーン/Object のCB確保（256Bアライン）
    sceneCB_ = BufferUtility::CreateUploadBuffer(device_.Get(), BufferUtility::AlignCB(sizeof(SceneCB)));
    objectCB_ = BufferUtility::CreateUploadBuffer(device_.Get(), BufferUtility::AlignCB(sizeof(ObjectCB)));

    sceneCB_->Map(0, nullptr, reinterpret_cast<void **>(&sceneCBMapped_));
    objectCB_->Map(0, nullptr, reinterpret_cast<void **>(&objectCBMapped_));
}

void ModelRenderer::Finalize() noexcept {
    if (sceneCB_)  sceneCB_->Unmap(0, nullptr);
    if (objectCB_) objectCB_->Unmap(0, nullptr);
    sceneCBMapped_ = nullptr;
    objectCBMapped_ = nullptr;
    objectCB_.Reset();
    sceneCB_.Reset();
    pso_.Reset();
    rootSig_.Reset();
    device_.Reset();
}

void ModelRenderer::Begin(ID3D12GraphicsCommandList *cmd,
    const float view[16], const float proj[16]) noexcept {
    // 共通CB を更新
    for (int i = 0; i < 16; ++i) { sceneCBMapped_->view[i] = view[i]; sceneCBMapped_->proj[i] = proj[i]; }

    // バインド
    cmd->SetGraphicsRootSignature(rootSig_.Get());
    cmd->SetPipelineState(pso_.Get());

    // シーンCB（b1）
    D3D12_GPU_VIRTUAL_ADDRESS scb = sceneCB_->GetGPUVirtualAddress();
    cmd->SetGraphicsRootConstantBufferView(1, scb);
}

void ModelRenderer::End(ID3D12GraphicsCommandList * /*cmd*/) noexcept {
    // いまは特に無し（将来: ライトやフォグの集約など）
}

void ModelRenderer::Draw(ID3D12GraphicsCommandList *cmd,
    const Mesh &mesh,
    const float world[16],
    const float color[4]) const noexcept {
    // オブジェクトCB（b0）
    for (int i = 0; i < 16; ++i) objectCBMapped_->world[i] = world[i];
    for (int i = 0; i < 4; ++i)  objectCBMapped_->color[i] = color[i];
    D3D12_GPU_VIRTUAL_ADDRESS ocb = objectCB_->GetGPUVirtualAddress();
    cmd->SetGraphicsRootConstantBufferView(0, ocb);

    // VB/IB
    const auto &vbv = mesh.GetVBV();
    const auto &ibv = mesh.GetIBV();
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd->IASetVertexBuffers(0, 1, &vbv);
    cmd->IASetIndexBuffer(&ibv);

    // ドロー
    cmd->DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
}

void ModelRenderer::CreateRootSignature() {
    // ルート：b0=ObjectCB, b1=SceneCB
    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].Descriptor.ShaderRegister = 0; // b0

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].Descriptor.ShaderRegister = 1; // b1

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &blob, &err);
    if (FAILED(hr) && err) OutputDebugStringA((char *)err->GetBufferPointer());
    assert(SUCCEEDED(hr));

    hr = device_->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig_));
    assert(SUCCEEDED(hr));
}

void ModelRenderer::CreatePipelineState() {
    // 入力レイアウト（pos, normal, uv）
    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // シェーダ読み込み（プロジェクト直下に置く想定。パスは適宜調整）
    auto vs = CompileHLSL(L"Resources/Shaders/ModelVS.hlsl", L"VSMain", L"vs_6_0");
    auto ps = CompileHLSL(L"Resources/Shaders/ModelPS.hlsl", L"PSMain", L"ps_6_0");

    D3D12_RASTERIZER_DESC rs{};
    rs.FillMode = D3D12_FILL_MODE_SOLID;
    rs.CullMode = D3D12_CULL_MODE_BACK; // 背面カリング
    rs.FrontCounterClockwise = FALSE;
    rs.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    D3D12_BLEND_DESC blend{};
    blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = rootSig_.Get();
    pso.InputLayout = {layout, _countof(layout)};
    pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
    pso.RasterizerState = rs;
    pso.BlendState = blend;
    pso.DepthStencilState = ds;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // あなたの RTV 設定に合わせる
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc = {1,0};
    pso.SampleMask = UINT_MAX;

    HRESULT hr = device_->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pso_));
    assert(SUCCEEDED(hr));
}
