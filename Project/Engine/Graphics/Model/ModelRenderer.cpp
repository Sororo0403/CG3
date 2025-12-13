#include "ModelRenderer.h"

#include "DirectX/DirectXCommon.h"
#include "PSO/PSOManager.h"
#include "Mesh/MeshManager.h"
#include "Model.h"
#include "Logger/Logger.h"
#include "DirectX/DirectXUtil.h"

#include <directx/d3dx12.h>
#include <cassert>
#include <cstring>
#include <DirectXMath.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

ModelRenderer::ModelRenderer(DirectXCommon *dx, PSOManager *psoManager,
                             MeshManager *meshManager)
    : dx_(dx), psoManager_(psoManager), meshManager_(meshManager) {
}

void ModelRenderer::Initialize() {
    CreateConstantBuffer();
}

void ModelRenderer::Begin() {
    auto *cmd = dx_->GetCommandList();

    cmd->SetPipelineState(psoManager_->GetModelPSO());
    cmd->SetGraphicsRootSignature(psoManager_->GetModelRoot());

    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    cbCursor_ = 0;
}

void ModelRenderer::Draw(const Model &model, const Camera *camera) {
    auto *cmd = dx_->GetCommandList();

    // Mesh 取得
    const D3D12_VERTEX_BUFFER_VIEW *vb = meshManager_->GetVBView(model.meshId);
    const D3D12_INDEX_BUFFER_VIEW *ib = meshManager_->GetIBView(model.meshId);

    uint32_t indexCount = meshManager_->GetIndexCount(model.meshId);

    if (!vb || !ib || indexCount == 0) {
        return;
    }

    // CB 残量チェック
    if (cbCursor_ + cbStride_ > cbCapacity_) {
        LOG_ERROR(
            "ModelRenderer: CB ring overflow. Increase kMaxModelsPerFrame.");
        return;
    }

    // World 
    const auto &t = model.transform;

    XMMATRIX world =
        XMMatrixScaling(t.scale.x, t.scale.y, t.scale.z) *
        XMMatrixRotationRollPitchYaw(t.rotation.x, t.rotation.y, t.rotation.z) *
        XMMatrixTranslation(t.position.x, t.position.y, t.position.z);

    XMMATRIX wvp = world * camera->GetViewProj();

    // CB 書き込み
    ModelCB cb{};
    XMStoreFloat4x4(&cb.wvp, XMMatrixTranspose(wvp));

    std::memcpy(mappedCB_ + cbCursor_, &cb, sizeof(cb));

    D3D12_GPU_VIRTUAL_ADDRESS cbGpu =
        constantBuffer_->GetGPUVirtualAddress() + cbCursor_;
    cbCursor_ += cbStride_;

    // Draw
    cmd->IASetVertexBuffers(0, 1, vb);
    cmd->IASetIndexBuffer(ib);

    // b0 : ModelCB
    cmd->SetGraphicsRootConstantBufferView(0, cbGpu);

    cmd->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
}

void ModelRenderer::CreateConstantBuffer() {
    LOG_INFO("ModelRenderer: CreateConstantBuffer");

    ID3D12Device *device = dx_->GetDevice();

    cbStride_ = DirectXUtil::Align256(static_cast<uint32_t>(sizeof(ModelCB)));
    cbCapacity_ = cbStride_ * kMaxModelsPerFrame;

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(cbCapacity_);

    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&constantBuffer_));
    assert(SUCCEEDED(hr));

    void *p = nullptr;
    hr = constantBuffer_->Map(0, nullptr, &p);
    assert(SUCCEEDED(hr) && p);

    mappedCB_ = reinterpret_cast<uint8_t *>(p);
    cbCursor_ = 0;
}
