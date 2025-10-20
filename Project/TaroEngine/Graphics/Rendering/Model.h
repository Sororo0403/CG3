#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Mesh.h"
#include "Transform.h"

/// <summary>
/// メッシュデータと変換情報を保持し、OBJ からロードして描画する 3D モデル。
/// </summary>
class Model {
public:
    // path は必須（空文字は不可）
    void Initialize(ID3D12Device *device, const std::string &path);

    Transform &GetTransform() noexcept { return transform_; }
    const Transform &GetTransform() const noexcept { return transform_; }

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return mesh_.GetVBV(); }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return mesh_.GetIBV(); }
    UINT GetIndexCount() const noexcept { return mesh_.GetIndexCount(); }

    const Mesh &GetMesh() const noexcept { return mesh_; }

private:
    void LoadFromOBJ_(ID3D12Device *device, const std::string &path);

private:
    Mesh      mesh_;
    Transform transform_;
};
