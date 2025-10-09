#include "TextureManager.h"
#include <cassert>

void TextureManager::Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap, UINT startIndex) {
    assert(device && srvHeap);
    device_ = device;
    srvHeap_ = srvHeap;
    nextIndex_ = startIndex;
    incSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

TextureHandle TextureManager::Load(ID3D12GraphicsCommandList *cmd, const std::wstring &pathW) {
    auto it = cache_.find(pathW);
    if (it != cache_.end()) return it->second;

    auto tex = std::make_shared<Texture2D>();
    tex->CreateFromFile(device_, cmd, pathW.c_str());

    // 既定：テクスチャのフォーマットそのまま（sRGB化済み）
    TextureView v = CreateSrvFor(*tex);

    TextureHandle h{tex, v};
    cache_.emplace(pathW, h);
    return h;
}

TextureView TextureManager::CreateSrvFor(const Texture2D &tex, const D3D12_SHADER_RESOURCE_VIEW_DESC *overrideDesc) {
    assert(device_ && srvHeap_);

    const UINT idx = nextIndex_++;
    TextureView v{};
    v.index = idx;
    v.cpu = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    v.cpu.ptr += static_cast<SIZE_T>(incSize_) * idx;
    v.gpu = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    v.gpu.ptr += static_cast<UINT64>(incSize_) * idx;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    if (overrideDesc) {
        srv = *overrideDesc;
    } else {
        srv.Format = tex.Format();
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MostDetailedMip = 0;
        srv.Texture2D.MipLevels = tex.MipLevels();
    }

    device_->CreateShaderResourceView(tex.GetResource(), &srv, v.cpu);
    return v;
}
