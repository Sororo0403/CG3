#include "TextureManager.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <cassert>
#include <vector>

#include "Logger/Logger.h"
#include "DirectX/DirectXCommon.h"
#include "directx/d3dx12.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

TextureManager::TextureManager(DirectXCommon *dx) : dx_(dx) {
    LOG_INFO("TextureManager constructed");
}

TextureManager::~TextureManager() {
    LOG_INFO("TextureManager destructor");

    textures_.clear();
    pathToId_.clear();
    dx_ = nullptr;
}

void TextureManager::Initialize() {
    LOG_INFO("TextureManager::Initialize start");

    assert(dx_ && "DirectXCommon must be provided via DI");
    nextIndex_ = 1;

    pathToId_.clear();
    textures_.clear();

    LOG_INFO("TextureManager::Initialize completed");
}

uint32_t TextureManager::LoadTexture(const std::string &filePath) {
    assert(dx_);

    LOG_INFO("LoadTexture: " + filePath);

    // すでに読み込み済みなら再利用
    auto it = pathToId_.find(filePath);
    if (it != pathToId_.end()) {
        return it->second;
    }

    // ファイル読み込み
    auto mipImages = LoadTextureFromFile(filePath);
    const TexMetadata &metadata = mipImages.GetMetadata();

    // GPU リソース作成
    auto texResource = CreateTextureResource(dx_->GetDevice(), metadata);

    // ピクセルデータのアップロード
    UploadTextureData(texResource.Get(), mipImages);

    // =============================
    // SRV 作成
    // =============================
    ID3D12Device *device = dx_->GetDevice();
    ID3D12DescriptorHeap *heap = dx_->GetSrvHeap();
    UINT descriptorSize = dx_->GetSrvDescriptorSize();

    uint32_t index = nextIndex_++;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle =
        heap->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += static_cast<SIZE_T>(index) * descriptorSize;

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle =
        heap->GetGPUDescriptorHandleForHeapStart();
    gpuHandle.ptr += static_cast<UINT64>(index) * descriptorSize;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);

    device->CreateShaderResourceView(texResource.Get(), &srvDesc, cpuHandle);

    // 管理テーブル登録
    Texture texInfo{};
    texInfo.resource = texResource;
    texInfo.gpuHandle = gpuHandle;

    textures_[index] = texInfo;
    pathToId_[filePath] = index;

    LOG_INFO("Texture loaded OK: " + filePath);
    return index;
}

const Texture &TextureManager::GetTexture(uint32_t id) const {
    return textures_.at(id);
}

ScratchImage TextureManager::LoadTextureFromFile(const std::string &filePath) {
    LOG_INFO("TextureManager::LoadTextureFromFile → " + filePath);

    ScratchImage image{};
    std::wstring pathW(filePath.begin(), filePath.end());

    HRESULT hr =
        LoadFromWICFile(pathW.c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, image);
    if (FAILED(hr)) {
        LOG_ERROR("LoadFromWICFile failed → " + filePath);
        assert(false);
    }

    // ミップ生成
    ScratchImage mipImages{};
    hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                         image.GetMetadata(), TEX_FILTER_SRGB, 0, mipImages);

    if (FAILED(hr)) {
        LOG_ERROR("GenerateMipMaps failed → " + filePath);
        assert(false);
    }

    return mipImages;
}

ComPtr<ID3D12Resource>
TextureManager::CreateTextureResource(ID3D12Device *device,
                                      const TexMetadata &metadata) {
    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format, metadata.width, static_cast<UINT>(metadata.height),
        static_cast<UINT16>(metadata.arraySize),
        static_cast<UINT16>(metadata.mipLevels));

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    ComPtr<ID3D12Resource> texture;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&texture));

    if (FAILED(hr)) {
        LOG_ERROR("CreateCommittedResource failed for texture");
        assert(false);
    }

    return texture;
}

void TextureManager::UploadTextureData(ID3D12Resource *texture,
                                       const ScratchImage &mipImages) {
    assert(dx_);

    ID3D12Device *device = dx_->GetDevice();
    ID3D12CommandQueue *queue = dx_->GetCommandQueue();
    const TexMetadata &meta = mipImages.GetMetadata();

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    subresources.reserve(meta.mipLevels);

    for (UINT mip = 0; mip < meta.mipLevels; ++mip) {
        const Image *img = mipImages.GetImage(mip, 0, 0);

        D3D12_SUBRESOURCE_DATA data{};
        data.pData = img->pixels;
        data.RowPitch = img->rowPitch;
        data.SlicePitch = img->slicePitch;

        subresources.push_back(data);
    }

    UINT64 uploadSize = GetRequiredIntermediateSize(
        texture, 0, static_cast<UINT>(meta.mipLevels));

    // Upload バッファ
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    ComPtr<ID3D12Resource> uploadBuffer;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&uploadBuffer));

    if (FAILED(hr)) {
        LOG_ERROR("Upload buffer creation failed");
        assert(false);
    }

    // 専用コマンドリストでコピー
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> cmdList;

    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                   IID_PPV_ARGS(&allocator));
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                              allocator.Get(), nullptr, IID_PPV_ARGS(&cmdList));

    UpdateSubresources(cmdList.Get(), texture, uploadBuffer.Get(), 0, 0,
                       static_cast<UINT>(meta.mipLevels), subresources.data());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture, D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);

    cmdList->Close();

    ID3D12CommandList *lists[] = {cmdList.Get()};
    queue->ExecuteCommandLists(1, lists);

    dx_->WaitForGpu();
}