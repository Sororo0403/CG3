#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "Texture2D.h"

struct TextureView {
    UINT index = UINT_MAX;
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
    bool IsValid() const { return index != UINT_MAX; }
};

struct TextureHandle {
    std::shared_ptr<Texture2D> resource;
    TextureView view; // 既定SRV
};

class TextureManager {
public:
    void Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap, UINT startIndex = 0);

    // 既存なら共有、無ければ読み込み＋既定SRV作成
    TextureHandle Load(ID3D12GraphicsCommandList *cmd, const std::wstring &pathW);

    // 任意フォーマット（非sRGBなど）でSRVだけ追加作成
    TextureView CreateSrvFor(const Texture2D &tex, const D3D12_SHADER_RESOURCE_VIEW_DESC *overrideDesc = nullptr);

private:
    ID3D12Device *device_ = nullptr;
    ID3D12DescriptorHeap *srvHeap_ = nullptr; // シェーダ可視なSRVヒープ
    UINT nextIndex_ = 0;
    UINT incSize_ = 0;

    std::unordered_map<std::wstring, TextureHandle> cache_; // path -> handle
};
