#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <DirectXMath.h>
#include "Mesh.h"
#include "Transform.h"

/// <summary>
/// OBJを読み込み、メッシュとテクスチャ情報を保持するモデル。
/// MTLのmap_Kdも覚えておく。外部でSRVを差し込む前提。
/// </summary>
class Model {
public:
    // path は必須（空文字は禁止）
    void Initialize(ID3D12Device *device, const std::string &path);

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return mesh_.GetVBV(); }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return mesh_.GetIBV(); }
    UINT GetIndexCount() const noexcept { return mesh_.GetIndexCount(); }

    const Mesh &GetMesh() const noexcept { return mesh_; }

    // === アルベドSRV ===
    void SetAlbedoSRV(D3D12_GPU_DESCRIPTOR_HANDLE h) noexcept { albedoSRV_ = h; hasAlbedo_ = true; }
    bool HasAlbedoSRV() const noexcept { return hasAlbedo_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetAlbedoSRV() const noexcept { return albedoSRV_; }

    // 読み取った map_Kd のパス（相対→結合済み）
    const std::string &GetAlbedoPath() const noexcept { return albedoPath_; }

    // === モデルローカルAABBへのアクセス ===
    DirectX::XMFLOAT3 GetLocalMin() const noexcept { return mesh_.GetMinPos(); }
    DirectX::XMFLOAT3 GetLocalMax() const noexcept { return mesh_.GetMaxPos(); }

    float GetLocalWidthX() const noexcept {
        auto mn = mesh_.GetMinPos();
        auto mx = mesh_.GetMaxPos();
        return mx.x - mn.x;
    }
    float GetLocalHeightY() const noexcept {
        auto mn = mesh_.GetMinPos();
        auto mx = mesh_.GetMaxPos();
        return mx.y - mn.y;
    }

private:
    void LoadFromOBJ_(ID3D12Device *device, const std::string &path);

private:
    Mesh mesh_;

    bool hasAlbedo_ = false;
    D3D12_GPU_DESCRIPTOR_HANDLE albedoSRV_{};
    std::string albedoPath_;
};
