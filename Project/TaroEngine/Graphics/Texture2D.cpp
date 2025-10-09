#include "Texture2D.h"
#include <cassert>
#include <vector>
#include <cstring>
#include "DirectXTex/DirectXTex.h"      // LoadFromWICFile, ScratchImage
#include "DirectXTex/d3dx12.h"          // CD3DX12_*

using Microsoft::WRL::ComPtr;
static inline void HR(HRESULT hr) { assert(SUCCEEDED(hr)); }

void Texture2D::CreateFromFile(
    ID3D12Device *device,
    ID3D12GraphicsCommandList *cmd,
    const wchar_t *pathW,
    ID3D12DescriptorHeap *srvHeap,
    UINT srvIndex) {
    using namespace DirectX;

    // 1) CPU 読み込み（sRGB 強制）
    ScratchImage img;
    TexMetadata meta{};
    HR(LoadFromWICFile(pathW, WIC_FLAGS_FORCE_SRGB, &meta, img));
    meta.format = MakeSRGB(meta.format);
    width_ = static_cast<uint32_t>(meta.width);
    height_ = static_cast<uint32_t>(meta.height);

    // 2) VRAM テクスチャ作成 (Default)
    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = (UINT)meta.width;
    texDesc.Height = (UINT)meta.height;
    texDesc.DepthOrArraySize = (UINT16)meta.arraySize;
    texDesc.MipLevels = (UINT16)meta.mipLevels;
    texDesc.Format = meta.format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapDefault{}; heapDefault.Type = D3D12_HEAP_TYPE_DEFAULT;
    HR(device->CreateCommittedResource(
        &heapDefault, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&tex_)));

    // 3) Copyable footprints を算出
    const UINT subCount = (UINT)(meta.mipLevels * meta.arraySize);
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(subCount);
    std::vector<UINT>    numRows(subCount);
    std::vector<UINT64>  rowSizes(subCount);
    UINT64 uploadSize = 0;
    device->GetCopyableFootprints(&texDesc, 0, subCount, 0,
        layouts.data(), numRows.data(), rowSizes.data(), &uploadSize);

    // Upload
    D3D12_HEAP_PROPERTIES heapUpload{}; heapUpload.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    HR(device->CreateCommittedResource(
        &heapUpload, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_)));

    // 4) CPU→Upload 書き込み
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

    // 5) Upload→VRAM へ Copy
    for (UINT i = 0; i < subCount; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = tex_.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = i;

        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = upload_.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    // 6) 遷移 COPY_DEST → PIXEL_SHADER_RESOURCE
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        tex_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmd->ResourceBarrier(1, &barrier);

    // 7) SRV を割り当て保持
    const UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    cpuSrv_ = srvHeap->GetCPUDescriptorHandleForHeapStart(); cpuSrv_.ptr += SIZE_T(inc) * srvIndex;
    gpuSrv_ = srvHeap->GetGPUDescriptorHandleForHeapStart(); gpuSrv_.ptr += UINT64(inc) * srvIndex;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = meta.format;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = (UINT)meta.mipLevels;

    device->CreateShaderResourceView(tex_.Get(), &srv, cpuSrv_);
}