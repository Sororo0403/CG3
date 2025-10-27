#define NOMINMAX
#include "ModelRenderer.h"
#include "ShaderCompiler.h"
#include "BufferUtility.h"
#include "Mesh.h"
#include "Model.h"
#include "Camera.h"
#include "DirectXCommon.h"

#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

void ModelRenderer::Initialize(ID3D12Device *device, ShaderCompiler *shader) {
    device_.Reset();
    device_ = device;
    shader_ = shader;

    CreateRootSignature();
    CreatePipelineState();

    // SceneCB: 1個ぶん
    sceneCB_ = BufferUtility::CreateUploadBuffer(
        device_.Get(),
        BufferUtility::AlignCB(sizeof(SceneCB))
    );

    // ObjectCB: フレーム数ぶん作る
    objectCBStride_ = BufferUtility::AlignCB(sizeof(ObjectCB));
    objectCBTotalBytes_ = objectCBStride_ * kMaxObjectsPerFrame;

    for (uint32_t i = 0; i < kFrameCount; ++i) {
        objectCBs_[i] = BufferUtility::CreateUploadBuffer(
            device_.Get(),
            objectCBTotalBytes_);
        objectCBOffset_[i] = 0;
    }

    currentFrameIndex_ = 0;
}

void ModelRenderer::Finalize() noexcept {
    if (sceneCB_) {
        sceneCB_->Unmap(0, nullptr);
    }
    sceneCB_.Reset();

    for (uint32_t i = 0; i < kFrameCount; ++i) {
        if (objectCBs_[i]) {
            objectCBs_[i]->Unmap(0, nullptr);
        }
        objectCBs_[i].Reset();
    }

    pso_.Reset();
    rootSig_.Reset();
    device_.Reset();
}

// Begin(view,proj)
void ModelRenderer::Begin(
    ID3D12GraphicsCommandList *commandList,
    DirectXCommon *dx,
    const XMMATRIX &view,
    const XMMATRIX &proj) noexcept {
    // 今フレームのインデックスを DirectXCommon から取得
    currentFrameIndex_ = dx->GetFrameIndex() % kFrameCount;

    // SceneCB 更新
    SceneCB scb{};
    XMStoreFloat4x4(&scb.view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&scb.proj, XMMatrixTranspose(proj));
    BufferUtility::WriteToUpload(
        sceneCB_.Get(),
        &scb,
        sizeof(SceneCB),
        0);

    // このフレーム用 ObjectCB リングをリセット
    objectCBOffset_[currentFrameIndex_] = 0;

    // PSO / RootSig セット
    commandList->SetGraphicsRootSignature(rootSig_.Get());
    commandList->SetPipelineState(pso_.Get());

    // b1: SceneCB
    commandList->SetGraphicsRootConstantBufferView(
        1,
        sceneCB_->GetGPUVirtualAddress());
}

// Begin(camera)
void ModelRenderer::Begin(
    ID3D12GraphicsCommandList *commandList,
    DirectXCommon *dx,
    const Camera &camera) noexcept {
    Begin(commandList, dx, camera.GetView(), camera.GetProj());
}

void ModelRenderer::End(ID3D12GraphicsCommandList *commandList) noexcept {
    (void)commandList;
}

void ModelRenderer::Draw(
    ID3D12GraphicsCommandList *commandList,
    const Model &model,
    const Transform &transform) noexcept {
    const Mesh &mesh = model.GetMesh();

    // 今フレーム用のCBを取り出す
    ComPtr<ID3D12Resource> &curCB = objectCBs_[currentFrameIndex_];

    // このオブジェクト分のスロットを確保
    const uint32_t myOffset = objectCBOffset_[currentFrameIndex_];
    objectCBOffset_[currentFrameIndex_] += objectCBStride_;
    assert(objectCBOffset_[currentFrameIndex_] <= objectCBTotalBytes_
        && "ObjectCB ring overflow in this frame");

    // 定数バッファデータを用意
    ObjectCB ocb{};
    const XMMATRIX W = transform.MakeWorldMatrix();
    XMStoreFloat4x4(&ocb.world, XMMatrixTranspose(W));
    ocb.color[0] = 1.0f;
    ocb.color[1] = 1.0f;
    ocb.color[2] = 1.0f;
    ocb.color[3] = 1.0f;
    ocb.useTexture = model.HasAlbedoSRV() ? 1u : 0u;

    // アップロードバッファへ書き込み
    BufferUtility::WriteToUpload(
        curCB.Get(),
        &ocb,
        sizeof(ObjectCB),
        myOffset);

    // b0: ObjectCB
    const auto base = curCB->GetGPUVirtualAddress();
    commandList->SetGraphicsRootConstantBufferView(
        0,
        base + myOffset);

    // t0: SRV (あるなら)
    if (model.HasAlbedoSRV()) {
        commandList->SetGraphicsRootDescriptorTable(
            2,
            model.GetAlbedoSRV());
    }

    // ドロー
    const auto &vbv = mesh.GetVBV();
    const auto &ibv = mesh.GetIBV();
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vbv);
    commandList->IASetIndexBuffer(&ibv);
    commandList->DrawIndexedInstanced(mesh.GetIndexCount(), 1, 0, 0, 0);
}

// RootSignature / PSO はオリジナルとほぼ同じ
void ModelRenderer::CreateRootSignature() {
    // b0: ObjectCB, b1: SceneCB, t0: SRV(Texture2D)

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0; // t0
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[3]{};
    // b0
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[0].Descriptor.ShaderRegister = 0;

    // b1
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    params[1].Descriptor.ShaderRegister = 1;

    // t0
    params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges = &srvRange;

    // s0 static sampler
    D3D12_STATIC_SAMPLER_DESC samp{};
    samp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = samp.AddressV = samp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    samp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    samp.ShaderRegister = 0; // s0

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = _countof(params);
    desc.pParameters = params;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &samp;
    desc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> blob, err;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &blob, &err);
    if (FAILED(hr) && err) {
        ::OutputDebugStringA((const char *)err->GetBufferPointer());
    }
    assert(SUCCEEDED(hr));

    hr = device_->CreateRootSignature(
        0,
        blob->GetBufferPointer(),
        blob->GetBufferSize(),
        IID_PPV_ARGS(&rootSig_));
    assert(SUCCEEDED(hr));
}

void ModelRenderer::CreatePipelineState() {
    assert(shader_ && "ShaderCompiler is null.");

    D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
            D3D12_APPEND_ALIGNED_ELEMENT,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    const bool kOptimize = true;
    const bool kDebug =
#ifdef _DEBUG
        true;
#else
        false;
#endif

    auto vsb = shader_->CompileFromFile(
        L"Resources/Shaders/ModelVS.hlsl", L"ModelVS", L"vs_6_0", {}, kOptimize, kDebug);
    auto psb = shader_->CompileFromFile(
        L"Resources/Shaders/ModelPS.hlsl", L"ModelPS", L"ps_6_0", {}, kOptimize, kDebug);
    assert(vsb && psb && "Shader compile failed.");

    D3D12_RASTERIZER_DESC rs{};
    rs.FillMode = D3D12_FILL_MODE_SOLID;
    rs.CullMode = D3D12_CULL_MODE_BACK;
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
    pso.VS = {vsb->GetBufferPointer(), vsb->GetBufferSize()};
    pso.PS = {psb->GetBufferPointer(), psb->GetBufferSize()};
    pso.RasterizerState = rs;
    pso.BlendState = blend;
    pso.DepthStencilState = ds;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc = {1, 0};
    pso.SampleMask = UINT_MAX;

    HRESULT hr = device_->CreateGraphicsPipelineState(
        &pso,
        IID_PPV_ARGS(&pso_));
    assert(SUCCEEDED(hr));
}
