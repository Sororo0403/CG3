#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Mesh.h"
#include "Transform.h"

/// <summary>
/// メッシュデータと変換情報を保持し、OBJ からロードして描画する 3D モデル。
/// MTL(map_Kd) の相対パスを覚えておき、外部でSRVを作って差し込める。
/// </summary>
class Model {
public:
    // path は必須（空文字は不可）
    void Initialize(ID3D12Device *device, const std::string &path);

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return mesh_.GetVBV(); }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return mesh_.GetIBV(); }
    UINT GetIndexCount() const noexcept { return mesh_.GetIndexCount(); }

    const Mesh &GetMesh() const noexcept { return mesh_; }

    // === テクスチャ（アルベド）SRV ===
    void SetAlbedoSRV(D3D12_GPU_DESCRIPTOR_HANDLE h) noexcept { albedoSRV_ = h; hasAlbedo_ = true; }
    bool HasAlbedoSRV() const noexcept { return hasAlbedo_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetAlbedoSRV() const noexcept { return albedoSRV_; }

    // 読み取った map_Kd の相対/結合パス（外部のロード時に利用）
    const std::string &GetAlbedoPath() const noexcept { return albedoPath_; }

private:
    void LoadFromOBJ_(ID3D12Device *device, const std::string &path);

private:
    Mesh mesh_;

    // テクスチャ関連
    bool hasAlbedo_ = false;
    D3D12_GPU_DESCRIPTOR_HANDLE albedoSRV_{};
    std::string albedoPath_;


};
