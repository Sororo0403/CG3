#pragma once
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXTex/DirectXTex.h"

class DirectXCommon;

class TextureManager {
public:
    struct Texture {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
    };

    static void Initialize(DirectXCommon *dx);
    static void Finalize();

    // 同じファイルは再ロードしない
    static uint32_t LoadTexture(const std::string &filePath);

    static const Texture &GetTexture(uint32_t id);

private:
    static DirectXCommon *s_dx_;
    static uint32_t s_nextIndex_;
    static std::unordered_map<std::string, uint32_t> s_pathToId_;
    static std::unordered_map<uint32_t, Texture> s_textures_;

    static DirectX::ScratchImage LoadTextureFromFile_(const std::string &filePath);
    static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource_(
        ID3D12Device *device, const DirectX::TexMetadata &metadata);
    static void UploadTextureData_(
        ID3D12Resource *texture, const DirectX::ScratchImage &mipImages);
};
