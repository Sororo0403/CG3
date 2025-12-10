#include "TextureManager.h"
#include "DirectX/DirectXCommon.h"
#include "Logger/Logger.h"
#include "directx/d3dx12.h"

#include <cassert>
#include <vector>

using namespace Microsoft::WRL;
using namespace DirectX;

TextureManager *TextureManager::GetInstance() {
  static TextureManager instance;
  return &instance;
}

TextureManager::~TextureManager() {
  LOG_INFO("TextureManager destructor: clearing textures");

  textures_.clear();
  pathToId_.clear();
  dx_ = nullptr;
}

void TextureManager::Initialize(DirectXCommon *dx) {
  LOG_INFO("TextureManager initialized");

  dx_ = dx;
  nextIndex_ = 1;
  pathToId_.clear();
  textures_.clear();
}

uint32_t TextureManager::LoadTexture(const std::string &filePath) {
  assert(dx_);

  LOG_INFO("LoadTexture: " + filePath);

  // 既にロード済みなら再利用
  if (auto it = pathToId_.find(filePath); it != pathToId_.end()) {
    LOG_DEBUG("Texture reused: " + filePath);
    return it->second;
  }

  LOG_DEBUG("Loading new texture: " + filePath);

  // 画像ファイル読み込み
  auto image = LoadTextureFromFile(filePath);
  const auto &metadata = image.GetMetadata();

  LOG_INFO("Image loaded. Size = " + std::to_string(metadata.width) + "x" +
           std::to_string(metadata.height) +
           " MipLevels = " + std::to_string(metadata.mipLevels));

  // GPUリソース生成
  auto tex = CreateTextureResource(dx_->GetDevice(), metadata);
  UploadTextureData(tex.Get(), image);

  LOG_INFO("GPU texture created successfully: " + filePath);

  // SRV登録
  ID3D12Device *device = dx_->GetDevice();
  ID3D12DescriptorHeap *heap = dx_->GetSrvHeap();
  UINT descriptorSize = dx_->GetSrvDescriptorSize();

  UINT index = nextIndex_++;

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

  device->CreateShaderResourceView(tex.Get(), &srvDesc, cpuHandle);

  LOG_DEBUG("SRV created for texture ID: " + std::to_string(index));

  Texture texInfo{};
  texInfo.resource = tex;
  texInfo.gpuHandle = gpuHandle;

  textures_[index] = texInfo;
  pathToId_[filePath] = index;

  LOG_INFO("Texture load completed: " + filePath);

  return index;
}

const Texture &TextureManager::GetTexture(uint32_t id) const {
  LOG_DEBUG("GetTexture: id = " + std::to_string(id));
  return textures_.at(id);
}

ScratchImage TextureManager::LoadTextureFromFile(const std::string &filePath) {
  LOG_DEBUG("LoadTextureFromFile: " + filePath);

  ScratchImage image{};
  std::wstring filePathW(filePath.begin(), filePath.end());

  HRESULT hr =
      LoadFromWICFile(filePathW.c_str(), WIC_FLAGS_FORCE_SRGB, nullptr, image);

  if (FAILED(hr)) {
    LOG_ERROR("LoadFromWICFile failed: " + filePath);
  }

  ScratchImage mipImages{};
  hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(),
                       image.GetMetadata(), TEX_FILTER_SRGB, 0, mipImages);

  if (FAILED(hr)) {
    LOG_ERROR("GenerateMipMaps failed: " + filePath);
  }

  LOG_DEBUG("MipMaps generated for " + filePath);

  return mipImages;
}

ComPtr<ID3D12Resource>
TextureManager::CreateTextureResource(ID3D12Device *device,
                                      const TexMetadata &metadata) {
  LOG_DEBUG("CreateTextureResource: " + std::to_string(metadata.width) + "x" +
            std::to_string(metadata.height));

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
  }

  return texture;
}

void TextureManager::UploadTextureData(ID3D12Resource *texture,
                                       const ScratchImage &mipImages) {
  LOG_DEBUG("UploadTextureData start");

  ID3D12Device *device = dx_->GetDevice();
  auto *queue = dx_->GetCommandQueue();

  const TexMetadata &meta = mipImages.GetMetadata();

  // SUBRESOURCE_DATA 作成
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  subresources.reserve(meta.mipLevels);

  for (UINT mip = 0; mip < static_cast<UINT>(meta.mipLevels); ++mip) {
    const Image *img = mipImages.GetImage(mip, 0, 0);

    D3D12_SUBRESOURCE_DATA data{};
    data.pData = img->pixels;
    data.RowPitch = img->rowPitch;
    data.SlicePitch = img->slicePitch;
    subresources.push_back(data);
  }

  // 必要なサイズ
  UINT64 uploadSize = GetRequiredIntermediateSize(
      texture, 0, static_cast<UINT>(meta.mipLevels));

  // UploadBuffer
  CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
  CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

  ComPtr<ID3D12Resource> uploadBuffer;
  HRESULT hr = device->CreateCommittedResource(
      &uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
  if (FAILED(hr)) {
    LOG_ERROR("CreateCommittedResource (UPLOAD) failed");
    return;
  }

  // 専用コマンドアロケータ＋リストを作る
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> cmdList;

  hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&allocator));
  assert(SUCCEEDED(hr));

  hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 allocator.Get(), nullptr,
                                 IID_PPV_ARGS(&cmdList));
  assert(SUCCEEDED(hr));

  // コピー + バリア
  UpdateSubresources(cmdList.Get(), texture, uploadBuffer.Get(), 0, 0,
                     static_cast<UINT>(meta.mipLevels), subresources.data());
  LOG_DEBUG("UpdateSubresources OK");

  CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
      texture, D3D12_RESOURCE_STATE_COPY_DEST,
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  cmdList->ResourceBarrier(1, &barrier);
  LOG_DEBUG("Texture barrier applied");

  // クローズして実行・完了待ち
  hr = cmdList->Close();
  assert(SUCCEEDED(hr));

  ID3D12CommandList *lists[] = {cmdList.Get()};
  queue->ExecuteCommandLists(1, lists);
  dx_->WaitForGpu();
}