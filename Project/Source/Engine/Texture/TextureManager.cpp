#include "TextureManager.h"
#include "DirectX/DirectXCommon.h"
#include <cassert>

using namespace Microsoft::WRL;
using namespace DirectX;

DirectXCommon *TextureManager::s_dx_ = nullptr;
uint32_t TextureManager::s_nextIndex_ = 1; // 0番はImGuiなど予約すると楽
std::unordered_map<std::string, uint32_t> TextureManager::s_pathToId_;
std::unordered_map<uint32_t, TextureManager::Texture> TextureManager::s_textures_;

void TextureManager::Initialize(DirectXCommon *dx) {
    s_dx_ = dx;
    s_nextIndex_ = 1;
    s_pathToId_.clear();
    s_textures_.clear();
}

void TextureManager::Finalize() {
    s_textures_.clear();
    s_pathToId_.clear();
    s_dx_ = nullptr;
}

uint32_t TextureManager::LoadTexture(const std::string &filePath) {
    assert(s_dx_);
    if (auto it = s_pathToId_.find(filePath); it != s_pathToId_.end()) {
        return it->second;
    }

    auto image = LoadTextureFromFile_(filePath);
    const auto &metadata = image.GetMetadata();
    auto tex = CreateTextureResource_(s_dx_->GetDevice(), metadata);
    UploadTextureData_(tex.Get(), image);

    ID3D12Device *device = s_dx_->GetDevice();
    ID3D12DescriptorHeap *heap = s_dx_->GetSrvHeap();
    UINT descriptorSize = s_dx_->GetSrvDescriptorSize();

    UINT index = s_nextIndex_++;
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(index) * descriptorSize;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += static_cast<UINT64>(index) * descriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

    device->CreateShaderResourceView(tex.Get(), &srvDesc, cpuHandle);

    Texture texInfo{};
    texInfo.resource = tex;
    texInfo.gpuHandle = gpuHandle;

    uint32_t id = index;
    s_textures_[id] = texInfo;
    s_pathToId_[filePath] = id;
    return id;
}

const TextureManager::Texture &TextureManager::GetTexture(uint32_t id) {
    return s_textures_.at(id);
}

// ==== ここから下は、元の大きなコードの関数をほぼそのまま移植 ==== 

ScratchImage TextureManager::LoadTextureFromFile_(const std::string &filePath) {
    ScratchImage image{};
    std::wstring filePathW(filePath.begin(), filePath.end());
    HRESULT hr = LoadFromWICFile(filePathW.c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, image);
    assert(SUCCEEDED(hr));

    ScratchImage mipImages{};
    hr = GenerateMipMaps(
        image.GetImages(), image.GetImageCount(), image.GetMetadata(),
        TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    return mipImages;
}

ComPtr<ID3D12Resource> TextureManager::CreateTextureResource_(
    ID3D12Device *device, const TexMetadata &metadata) {
    D3D12_RESOURCE_DESC desc{};
    desc.Width = static_cast<UINT>(metadata.width);
    desc.Height = static_cast<UINT>(metadata.height);
    desc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
    desc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
    desc.Format = metadata.format;
    desc.SampleDesc.Count = 1;
    desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_CUSTOM;
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}

void TextureManager::UploadTextureData_(
    ID3D12Resource *texture, const ScratchImage &mipImages) {
    const TexMetadata &meta = mipImages.GetMetadata();
    for (size_t mip = 0; mip < meta.mipLevels; ++mip) {
        const Image *img = mipImages.GetImage(mip, 0, 0);
        HRESULT hr = texture->WriteToSubresource(
            static_cast<UINT>(mip),
            nullptr,
            img->pixels,
            static_cast<UINT>(img->rowPitch),
            static_cast<UINT>(img->slicePitch));
        assert(SUCCEEDED(hr));
    }
}
