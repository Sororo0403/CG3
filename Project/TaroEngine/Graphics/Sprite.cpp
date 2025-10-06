#include "Sprite.h"
#include <cassert>
#include <cstring>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Uploadバッファ作成ヘルパ
static ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device *device, size_t bytes) {
    ComPtr<ID3D12Resource> res;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(res.ReleaseAndGetAddressOf()));
    assert(SUCCEEDED(hr));
    return res;
}

void Sprite::Initialize(ID3D12Device *device) {
    assert(device);
    device_ = device;
    CreateBuffers(device_);
    SetColor(1, 1, 1, 1); // 既定：白
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
    mappedMaterial_->color = {r,g,b,a};
    // それ以外の Material フィールドは既定値
    mappedMaterial_->enableLighting = 0;
    mappedMaterial_->uvTransform = {}; // 未使用
}

void Sprite::Update() {
    UpdateConstants();
}

void Sprite::Draw(ID3D12GraphicsCommandList *cmd) const {
    // ルートバインド
    // p0: PS CBV(b0), p1: VS CBV(b1)
    cmd->SetGraphicsRootConstantBufferView(0, cbMaterial_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, cbTransform_->GetGPUVirtualAddress());

    // VB/IB
    cmd->IASetVertexBuffers(0, 1, &vbv_);
    cmd->IASetIndexBuffer(&ibv_);

    // 2トライアングル
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateBuffers(ID3D12Device *device) {
    // 頂点4 / インデックス6
    vb_ = CreateUploadBuffer(device, sizeof(VertexData) * 4);
    ib_ = CreateUploadBuffer(device, sizeof(uint32_t) * 6);

    // マップ
    vb_->Map(0, nullptr, reinterpret_cast<void **>(&mappedVB_));
    ib_->Map(0, nullptr, reinterpret_cast<void **>(&mappedIB_));

    // インデックス（2三角形）
    uint32_t idx[6] = {0,1,2, 2,1,3};
    std::memcpy(mappedIB_, idx, sizeof(idx));

    // VBV/IBV（ストライドは VertexData のサイズ）
    vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = sizeof(VertexData);
    vbv_.SizeInBytes = sizeof(VertexData) * 4;

    ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibv_.Format = DXGI_FORMAT_R32_UINT;
    ibv_.SizeInBytes = sizeof(uint32_t) * 6;

    // CB
    cbMaterial_ = CreateUploadBuffer(device, ((sizeof(Material) + 255) / 256) * 256);
    cbTransform_ = CreateUploadBuffer(device, ((sizeof(TransformMatrix) + 255) / 256) * 256);

    cbMaterial_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
    cbTransform_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // 初期矩形
    SetRect(0, 0, 100, 100);
}

void Sprite::UpdateVertices() {
    // 左上原点のピクセル矩形をそのまま頂点へ（z=0, w=1）
    // [0] 左上, [1] 右上, [2] 左下, [3] 右下
    // UV は 0-1 固定（PS は gColor 固定出力だが将来の拡張のため保持）
    VertexData v{};
    // 0: 左上
    mappedVB_[0].position = {x_,        y_,        0.0f, 1.0f};
    mappedVB_[0].texcoord = {0.0f, 0.0f};
    mappedVB_[0].normal = {0.0f, 0.0f, 1.0f};
    // 1: 右上
    mappedVB_[1].position = {x_ + w_,     y_,        0.0f, 1.0f};
    mappedVB_[1].texcoord = {1.0f, 0.0f};
    mappedVB_[1].normal = {0.0f, 0.0f, 1.0f};
    // 2: 左下
    mappedVB_[2].position = {x_,        y_ + h_,     0.0f, 1.0f};
    mappedVB_[2].texcoord = {0.0f, 1.0f};
    mappedVB_[2].normal = {0.0f, 0.0f, 1.0f};
    // 3: 右下
    mappedVB_[3].position = {x_ + w_,     y_ + h_,     0.0f, 1.0f};
    mappedVB_[3].texcoord = {1.0f, 1.0f};
    mappedVB_[3].normal = {0.0f, 0.0f, 1.0f};
}

static inline void StoreMatrixColumnMajor(float dst[16], const XMFLOAT4X4 &m) {
    // XMFLOAT4X4 は row-major 配列だが、HLSL 既定 column_major に合わせて転置して送る
    // （あなたの Matrix4x4 実装に合わせてここは自由に差し替え可）
    XMMATRIX mt = XMMatrixTranspose(XMLoadFloat4x4(&m));
    XMFLOAT4X4 cm{};
    XMStoreFloat4x4(&cm, mt);
    std::memcpy(dst, &cm, sizeof(cm));
}

TransformMatrix Sprite::MakePixelToNDC(uint32_t vpw, uint32_t vph) {
    // ピクセル座標 (0..W, 0..H) → NDC (-1..1, 1..-1)
    //  [ 2/W   0   0   0 ]
    //  [ 0   -2/H  0   0 ]
    //  [ 0     0   1   0 ]
    //  [-1     1   0   1 ]
    XMFLOAT4X4 M = {
        2.0f / vpw, 0,            0, 0,
        0,         -2.0f / vph,   0, 0,
        0,          0,            1, 0,
       -1.0f,       1.0f,         0, 1
    };

    TransformMatrix t{};
    // WVP = 上の行列、World は恒等でOK
    StoreMatrixColumnMajor(reinterpret_cast<float *>(&t.WVP), M);

    XMFLOAT4X4 I = {
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1
    };
    StoreMatrixColumnMajor(reinterpret_cast<float *>(&t.World), I);
    return t;
}

void Sprite::UpdateConstants() {
    if (!mappedTransform_) return;
    TransformMatrix proj = MakePixelToNDC(viewportW_, viewportH_);
    *mappedTransform_ = proj;
}
