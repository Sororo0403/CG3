#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "Texture2D.h"

class TextureManager {
public:

    void Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap, UINT startIndex = 0) {
        device_ = device; srvHeap_ = srvHeap; nextIndex_ = startIndex;
    }

    // 既存なら共有、無ければ読み込み
    std::shared_ptr<Texture2D> Load(ID3D12GraphicsCommandList *cmd, const std::wstring &pathW) {
        auto it = cache_.find(pathW);
        if (it != cache_.end()) return it->second;

        auto tex = std::make_shared<Texture2D>();
        tex->CreateFromFile(device_, cmd, pathW.c_str(), srvHeap_, nextIndex_++);
        cache_.emplace(pathW, tex);
        return tex;
    }

private:
    ID3D12Device *device_ = nullptr;
    ID3D12DescriptorHeap *srvHeap_ = nullptr;
    UINT nextIndex_ = 0;
    std::unordered_map<std::wstring, std::shared_ptr<Texture2D>> cache_;
};
