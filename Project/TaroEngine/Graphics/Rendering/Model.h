#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include "Mesh.h"
#include "Transform.h"

/// <summary>
/// メッシュデータと変換情報を保持し、描画を行う 3D モデルクラス。<br/>
/// OBJ からのロード（簡易）と、フォールバックとしてのボックス生成に対応します。
/// </summary>
class Model {
public:
    void Initialize(ID3D12Device *device, const std::string &path);
    void Draw(ID3D12GraphicsCommandList *commandList) const;

    Transform &GetTransform() noexcept { return transform_; }
    const Transform &GetTransform() const noexcept { return transform_; }

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return mesh_.GetVBV(); }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return mesh_.GetIBV(); }
    UINT GetIndexCount() const noexcept { return mesh_.GetIndexCount(); }

private:
    void CreateFromBox_(ID3D12Device *device);
    void LoadFromOBJ_(ID3D12Device *device, const std::string &path);

private:
    Mesh      mesh_;
    Transform transform_;
};