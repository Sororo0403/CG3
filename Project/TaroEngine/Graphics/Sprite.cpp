#include "Sprite.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include "DirectXTex/d3dx12.h"
#include "DirectXTex/DirectXTex.h"
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

void Sprite::Draw(ID3D12GraphicsCommandList *cmd) const {
    // p0: PS CBV(b0), p1: VS CBV(b1), p2: SRV(t0)
    cmd->SetGraphicsRootConstantBufferView(0, cbMaterial_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, cbTransform_->GetGPUVirtualAddress());
    if (gpuSrv_.ptr) { cmd->SetGraphicsRootDescriptorTable(2, gpuSrv_); }

    cmd->IASetVertexBuffers(0, 1, &vbv_);
    cmd->IASetIndexBuffer(&ibv_);
    cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateBuffers(ID3D12Device *device) {
    // 頂点4 / インデックス6
    vb_ = BufferUtil::CreateUploadBuffer(device, sizeof(VertexData) * 4);
    ib_ = BufferUtil::CreateUploadBuffer(device, sizeof(uint32_t) * 6);

    // マップ
    vb_->Map(0, nullptr, reinterpret_cast<void **>(&mappedVB_));
    ib_->Map(0, nullptr, reinterpret_cast<void **>(&mappedIB_));

    // インデックス（2三角形）
    uint32_t idx[6] = {0,1,2,  2,1,3};
    std::memcpy(mappedIB_, idx, sizeof(idx));

    // VBV/IBV
    vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = sizeof(VertexData);
    vbv_.SizeInBytes = sizeof(VertexData) * 4;

    ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibv_.Format = DXGI_FORMAT_R32_UINT;
    ibv_.SizeInBytes = sizeof(uint32_t) * 6;

    // CB（256 アライン）
    const size_t matSize = ((sizeof(Material) + 255) & ~size_t(255));
    const size_t trnSize = ((sizeof(TransformMatrix) + 255) & ~size_t(255));
    cbMaterial_ = BufferUtil::CreateUploadBuffer(device, matSize);
    cbTransform_ = BufferUtil::CreateUploadBuffer(device, trnSize);
    cbMaterial_->Map(0, nullptr, reinterpret_cast<void **>(&mappedMaterial_));
    cbTransform_->Map(0, nullptr, reinterpret_cast<void **>(&mappedTransform_));

    // 初期矩形
    SetRect(0, 0, 100, 100);
}

void Sprite::UpdateVertices() {
    // 左上原点のピクセル矩形 → 頂点へ
    mappedVB_[0].position = {x_,      y_,      0.0f, 1.0f}; // 左上
    mappedVB_[0].texcoord = {0.0f, 0.0f};
    mappedVB_[0].normal = {0,0,1};

    mappedVB_[1].position = {x_ + w_,   y_,      0.0f, 1.0f}; // 右上
    mappedVB_[1].texcoord = {1.0f, 0.0f};
    mappedVB_[1].normal = {0,0,1};

    mappedVB_[2].position = {x_,      y_ + h_,   0.0f, 1.0f}; // 左下
    mappedVB_[2].texcoord = {0.0f, 1.0f};
    mappedVB_[2].normal = {0,0,1};

    mappedVB_[3].position = {x_ + w_,   y_ + h_,   0.0f, 1.0f}; // 右下
    mappedVB_[3].texcoord = {1.0f, 1.0f};
    mappedVB_[3].normal = {0,0,1};
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

// ===== 画像→Upload→Copy→SRV =====
void Sprite::LoadTextureFromFile(ID3D12Device *device,
    ID3D12GraphicsCommandList *cmd,
    const wchar_t *pathW,
    ID3D12DescriptorHeap *srvHeap,
    UINT srvIndex) {

    using namespace DirectX; // DirectXTex の名前空間

    // 1) 画像をCPUで読む（sRGB扱い）
    ScratchImage img;
    TexMetadata meta{};
    CheckHR(LoadFromWICFile(pathW, WIC_FLAGS_FORCE_SRGB, &meta, img));
    meta.format = MakeSRGB(meta.format);

    // 2) VRAM(Default)にテクスチャを作成
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<UINT>(meta.width);
    texDesc.Height = static_cast<UINT>(meta.height);
    texDesc.DepthOrArraySize = static_cast<UINT16>(meta.arraySize);
    texDesc.MipLevels = static_cast<UINT16>(meta.mipLevels);
    texDesc.Format = meta.format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapDefault{};
    heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;

    CheckHR(device->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&tex_)));

    // 3) Uploadレイアウト計算
    const UINT subCount = static_cast<UINT>(meta.mipLevels * meta.arraySize);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subCount);
    std::vector<UINT>   numRows(subCount);
    std::vector<UINT64> rowSizes(subCount);
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, subCount, 0,
        layouts.data(), numRows.data(),
        rowSizes.data(), &uploadSize);

    // Uploadバッファ
    D3D12_HEAP_PROPERTIES heapUpload{};
    heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    CheckHR(device->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&texUpload_)));

    // 4) CPUでUploadへ書き込み
    const Image *images = img.GetImages();
    uint8_t *mapped = nullptr;
    CheckHR(texUpload_->Map(0, nullptr, reinterpret_cast<void **>(&mapped)));
    for (UINT i = 0; i < subCount; ++i) {
        const auto &lay = layouts[i];
        const auto *src = images + i;
        uint8_t *dst = mapped + lay.Offset;
        const size_t dstPitch = lay.Footprint.RowPitch;
        const size_t srcPitch = src->rowPitch;
        const UINT rows = numRows[i];
        for (UINT y = 0; y < rows; ++y) {
            std::memcpy(dst + y * dstPitch,
                src->pixels + y * srcPitch,
                (std::min)(dstPitch, srcPitch));
        }
    }
    texUpload_->Unmap(0, nullptr);

    // 5) CopyTextureRegionでVRAMへ転送
    for (UINT i = 0; i < subCount; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = tex_.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = i;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = texUpload_.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    // 6) PS用に遷移
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = tex_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmd->ResourceBarrier(1, &barrier);

    // 7) SRV作成（srvIndex に配置）
    const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuSrv_ = srvHeap->GetCPUDescriptorHandleForHeapStart();
    cpuSrv_.ptr += SIZE_T(inc) * srvIndex;
    gpuSrv_ = srvHeap->GetGPUDescriptorHandleForHeapStart();
    gpuSrv_.ptr += UINT64(inc) * srvIndex;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = meta.format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = (UINT)meta.mipLevels;

    device->CreateShaderResourceView(tex_.Get(), &srv, cpuSrv_);
}
