#include "Texture2D.h"
#include <cassert>
#include <vector>
#include <cstring>

// DirectXTex
#include "DirectXTex/DirectXTex.h"
#include "DirectXTex/d3dx12.h"

using Microsoft::WRL::ComPtr;
static inline void HR(HRESULT hr) { assert(SUCCEEDED(hr)); }

void Texture2D::CreateFromFile(
    ID3D12Device *device,
    ID3D12GraphicsCommandList *cmd,
    const wchar_t *pathW) {
    using namespace DirectX;

    // --- 読み込み（sRGB化） ---
    ScratchImage img;
    TexMetadata meta{};
    HR(LoadFromWICFile(pathW, WIC_FLAGS_FORCE_SRGB, &meta, img));
    meta.format = MakeSRGB(meta.format);

    width_ = static_cast<uint32_t>(meta.width);
    height_ = static_cast<uint32_t>(meta.height);
    format_ = meta.format;
    mipLevels_ = static_cast<UINT>(meta.mipLevels);
    arraySize_ = static_cast<UINT>(meta.arraySize);

    // --- Default リソースを作成 ---
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<UINT>(meta.width);
    texDesc.Height = static_cast<UINT>(meta.height);
    texDesc.DepthOrArraySize = static_cast<UINT16>(meta.arraySize);
    texDesc.MipLevels = static_cast<UINT16>(meta.mipLevels);
    texDesc.Format = meta.format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapDefault{}; heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
    HR(device->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex_)));

    // --- Upload のレイアウト計算 ---
    const UINT subCount = static_cast<UINT>(meta.mipLevels * meta.arraySize);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subCount);
    std::vector<UINT>   numRows(subCount);
    std::vector<UINT64> rowSizes(subCount);
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, subCount, 0,
        layouts.data(), numRows.data(), rowSizes.data(), &uploadSize);

    // --- Upload リソース作成＆コピー ---
    D3D12_HEAP_PROPERTIES heapUpload{}; heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC upDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    HR(device->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &upDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_)));

    const Image *images = img.GetImages();
    uint8_t *mapped = nullptr;
    HR(upload_->Map(0, nullptr, reinterpret_cast<void **>(&mapped)));
    for (UINT i = 0; i < subCount; ++i) {
        const auto &lay = layouts[i];
        const auto *src = images + i;
        auto *dst = mapped + lay.Offset;
        const size_t dstPitch = lay.Footprint.RowPitch;
        const size_t srcPitch = src->rowPitch;
        const UINT rows = numRows[i];
        for (UINT y = 0; y < rows; ++y) {
            std::memcpy(dst + y * dstPitch, src->pixels + y * srcPitch, (std::min)(dstPitch, srcPitch));
        }
    }
    upload_->Unmap(0, nullptr);

    // --- Copy: Upload -> Default ---
    for (UINT i = 0; i < subCount; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst{tex_.Get(), D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        dst.SubresourceIndex = i;
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = upload_.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    // --- Barrier: COPY_DEST -> PIXEL_SHADER_RESOURCE ---
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        tex_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);
}
