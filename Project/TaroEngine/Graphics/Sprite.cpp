#include "Sprite.h"
#include "BufferUtil.h"
#include "MatrixUtil.h"
#include "Camera.h"
#include <cassert>
#include <cstring>

// HLSL 側が row_major なら 1（転置せず送る）
// 既定の column_major なら 0（送る直前に Transpose）
#define SPRITE_SEND_ROW_MAJOR 0

void Sprite::Initialize(ID3D12Device *device) {
    assert(device);

    // === 頂点 / インデックス ===
    vertexResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(VertexData) * 4);
    indexResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(uint32_t) * 6);

    vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData_));
    indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData_));

    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

    // 四角形のインデックス
    const uint32_t indices[6] = {0,1,2, 2,1,3};
    std::memcpy(indexData_, indices, sizeof(indices));

    // === マテリアル ===
    materialResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
    materialData_->color = Vector4(1, 1, 1, 1);
    materialData_->enableLighting = false;
    materialData_->uvTransform = MatrixUtil::MakeIdentityMatrix();

    // === 変換行列 ===
    transformResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(TransformMatrix));
    transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&transformMatrixData_));
    transformMatrixData_->World = MatrixUtil::MakeIdentityMatrix();
    transformMatrixData_->WVP = MatrixUtil::MakeIdentityMatrix();
}

// 内部共通：vp = View * Proj を受け取り、WVP を組む
void Sprite::UpdateImpl_(const Matrix4x4 &vp) {
    // 頂点（ローカル）更新：サイズは頂点段階で反映
    const float hw = size_.x * 0.5f;
    const float hh = size_.y * 0.5f;

    vertexData_[0] = {{ -hw, -hh, 0.0f }, { 0.0f, 1.0f }};
    vertexData_[1] = {{ -hw,  hh, 0.0f }, { 0.0f, 0.0f }};
    vertexData_[2] = {{  hw, -hh, 0.0f }, { 1.0f, 1.0f }};
    vertexData_[3] = {{  hw,  hh, 0.0f }, { 1.0f, 0.0f }};

    // World：回転→平行移動（スケールは頂点で済ませたので掛けない）
    auto R = MatrixUtil::MakeRotationZMatrix(rotation_);
    auto T = MatrixUtil::MakeTranslationMatrix(position_.x, position_.y, 0.0f);
    auto W = MatrixUtil::Multiply(R, T);

#if SPRITE_SEND_ROW_MAJOR
    transformMatrixData_->World = W;
    transformMatrixData_->WVP = MatrixUtil::Multiply(W, vp);
#else
    transformMatrixData_->World = MatrixUtil::Transpose(W);
    auto wvp = MatrixUtil::Multiply(W, vp);
    transformMatrixData_->WVP = MatrixUtil::Transpose(wvp);
#endif
}

// 互換用：固定の正射影(1280x720)で VP を組む
void Sprite::Update() {
    auto V = MatrixUtil::MakeIdentityMatrix();
    auto P = MatrixUtil::MakeOrthographicMatrix(1280.0f, 720.0f, 0.0f, 1.0f);
    auto VP = MatrixUtil::Multiply(V, P);
    UpdateImpl_(VP);
}

// カメラ使用版：VP = View * Proj（右掛け）
void Sprite::Update(const Camera &camera) {
    const auto &V = camera.GetView();
    const auto &P = camera.GetProjection();
    auto VP = MatrixUtil::Multiply(V, P);
    UpdateImpl_(VP);
}

void Sprite::Draw(ID3D12GraphicsCommandList *cmdList) {
    assert(cmdList);

    cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    cmdList->IASetIndexBuffer(&indexBufferView_);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // RootParameter の順に CBV を積む（例：b0=Material, b1=Transform）
    cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());

    cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
