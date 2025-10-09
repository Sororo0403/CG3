#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() = default;

    // 画像を読み込み、Defaultリソースへコピー（SRVは作らない）
    void CreateFromFile(
        ID3D12Device *device,
        ID3D12GraphicsCommandList *cmd,
        const wchar_t *pathW);

    ID3D12Resource *GetResource() const { return tex_.Get(); }
    uint32_t        Width()  const { return width_; }
    uint32_t        Height() const { return height_; }
    DXGI_FORMAT     Format() const { return format_; }
    UINT            MipLevels() const { return mipLevels_; }
    UINT            ArraySize() const { return arraySize_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> tex_;     // Default (VRAM)
    Microsoft::WRL::ComPtr<ID3D12Resource> upload_;  // 一時Upload

    uint32_t    width_ = 0, height_ = 0;
    DXGI_FORMAT format_ = DXGI_FORMAT_UNKNOWN;
    UINT        mipLevels_ = 1;
    UINT        arraySize_ = 1;
};