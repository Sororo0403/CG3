#include "Sprite.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include "DirectXTex/d3dx12.h"
#include "BufferUtil.h"
#include "Matrix4x4.h"
#include "MatrixUtil.h"

using Microsoft::WRL::ComPtr;
static inline void CheckHR(HRESULT hr) { assert(SUCCEEDED(hr)); }

// HLSL 既定(column_major)で読むなら、転置して送る
static inline void StoreTransposedAsRowArray(float dst[16], const Matrix4x4 &src) {
    Matrix4x4 t{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            t.m[r][c] = src.m[c][r]; // transpose
    std::memcpy(dst, &t, sizeof(t));
}

void Sprite::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;
    CreateBuffers(device_);
    SetColor(1, 1, 1, 1);
}

void Sprite::SetViewportSize(uint32_t w, uint32_t h) {
    viewportW_ = (w == 0 ? 1u : w);
    viewportH_ = (h == 0 ? 1u : h);
}

void Sprite::SetRect(float x, float y, float width, float height) {
    x_ = x; y_ = y; w_ = width; h_ = height;
    UpdateVertices();
}

void Sprite::SetColor(float r, float g, float b, float a) {
    if (!mappedMaterial_) return;
    mappedMaterial_->color = {r, g, b, a};
    mappedMaterial_->enableLighting = 0;
    mappedMaterial_->uvTransform = {};
}

void Sprite::Update() { UpdateConstants(); }

void Sprite::SetTextureView(const TextureView &view) {
    hasTexture_ = view.IsValid();
    if (hasTexture_) {
        srvGpu_ = view.gpu;
    }
}

void Sprite::SetTextureHandle(D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
    hasTexture_ = (gpuHandle.ptr != 0);
    srvGpu_ = gpuHandle;
}

void Sprite::Draw(ID3D12GraphicsCommandList *cmd) const {
    // 注意：呼び出し側でシェーダ可視なSRVヒープを SetDescriptorHeaps 済みであること
    // p0: PS CBV(b0), p1: VS CBV(b1), p2: SRV(t0)
    cmd->SetGraphicsRootConstantBufferView(0, cbMaterial_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, cbTransform_->GetGPUVirtualAddress());
    if (hasTexture_) {
        cmd->SetGraphicsRootDescriptorTable(2, srvGpu_);
    }

    cmd->IASetVertexBuffers(0, 1, &vbv_);
    cmd->IASetIndexBuffer(&ibv_);
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateBuffers(ID3D12Device *device) {
    assert(device);

    // 頂点・インデックスバッファ作成（Upload）
    vb_ = BufferUtil::CreateUploadBuffer(device, sizeof(VertexData) * 4);
    ib_ = BufferUtil::CreateUploadBuffer(device, sizeof(uint32_t) * 6);

    // マップしてデータ書き込み
    vb_->Map(0, nullptr, reinterpret_cast<void **>(&mappedVB_));
    ib_->Map(0, nullptr, reinterpret_cast<void **>(&mappedIB_));

    // インデックス（2三角形）
    const uint32_t indices[6] = {0, 1, 2, 2, 1, 3};
    std::memcpy(mappedIB_, indices, sizeof(indices));

    // VBV/IBV を BufferUtil のヘルパで生成
    vbv_ = BufferUtil::MakeVBV(vb_.Get(), sizeof(VertexData), sizeof(VertexData) * 4);
    ibv_ = BufferUtil::MakeIBV(ib_.Get(), sizeof(uint32_t) * 6);

    // 定数バッファ（256B アライン）
    const UINT matSize = BufferUtil::AlignConstantBufferSize(sizeof(Material));
    const UINT trnSize = BufferUtil::AlignConstantBufferSize(sizeof(TransformMatrix));

    cbMaterial_ = BufferUtil::CreateUploadBuffer(device, matSize);
    cbTransform_ = BufferUtil::CreateUploadBuffer(device, trnSize);

    cbMaterial_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
    cbTransform_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // 初期矩形設定
    SetRect(0, 0, 100, 100);
}

void Sprite::UpdateVertices() {
    // 左上原点のピクセル矩形 → 頂点へ
    mappedVB_[0].position = {x_,       y_,        0.0f, 1.0f}; // 左上
    mappedVB_[0].texcoord = {0.0f, 0.0f};
    mappedVB_[0].normal = {0, 0, 1};

    mappedVB_[1].position = {x_ + w_,  y_,        0.0f, 1.0f}; // 右上
    mappedVB_[1].texcoord = {1.0f, 0.0f};
    mappedVB_[1].normal = {0, 0, 1};

    mappedVB_[2].position = {x_,       y_ + h_,   0.0f, 1.0f}; // 左下
    mappedVB_[2].texcoord = {0.0f, 1.0f};
    mappedVB_[2].normal = {0, 0, 1};

    mappedVB_[3].position = {x_ + w_,  y_ + h_,   0.0f, 1.0f}; // 右下
    mappedVB_[3].texcoord = {1.0f, 1.0f};
    mappedVB_[3].normal = {0, 0, 1};
}

TransformMatrix Sprite::MakePixelToNDC(uint32_t vpw, uint32_t vph) {
    // x_ndc =  (2/vpw)*x - 1
    // y_ndc = -(2/vph)*y + 1
    Matrix4x4 M = MatrixUtil::MakeIdentityMatrix();
    M.m[0][0] = 2.0f / static_cast<float>(vpw);
    M.m[1][1] = -2.0f / static_cast<float>(vph);
    M.m[2][2] = 1.0f;
    M.m[3][0] = -1.0f;
    M.m[3][1] = 1.0f;
    M.m[3][3] = 1.0f;

    TransformMatrix t{};
    StoreTransposedAsRowArray(reinterpret_cast<float *>(&t.WVP), M);
    StoreTransposedAsRowArray(reinterpret_cast<float *>(&t.World), MatrixUtil::MakeIdentityMatrix());
    return t;
}

void Sprite::UpdateConstants() {
    if (!mappedTransform_) return;
    *mappedTransform_ = MakePixelToNDC(viewportW_, viewportH_);
}
