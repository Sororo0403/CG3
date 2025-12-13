#pragma once

#include <unordered_map>
#include <memory>
#include <cstdint>

#include <d3d12.h>
#include <wrl.h>

#include "Mesh.h"

/// <summary>
/// Mesh を管理するクラス
/// </summary>
class MeshManager {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    MeshManager(ID3D12Device *device);

    /// <summary>
    /// CPU Mesh を登録し、GPU バッファを生成
    /// </summary>
    uint32_t Register(std::unique_ptr<Mesh> mesh);

    // Getter
    const D3D12_VERTEX_BUFFER_VIEW *GetVBView(uint32_t id) const;
    const D3D12_INDEX_BUFFER_VIEW *GetIBView(uint32_t id) const;
    uint32_t GetIndexCount(uint32_t id) const;

  private:
    void CreateGpuBuffers(uint32_t id, const Mesh &mesh);

  private:
    ID3D12Device *device_ = nullptr;

    // CPU Mesh
    std::unordered_map<uint32_t, std::unique_ptr<Mesh>> meshes_;

    // GPU Resources
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D12Resource>>
        vertexBuffers_;
    std::unordered_map<uint32_t, Microsoft::WRL::ComPtr<ID3D12Resource>>
        indexBuffers_;

    std::unordered_map<uint32_t, D3D12_VERTEX_BUFFER_VIEW> vbViews_;
    std::unordered_map<uint32_t, D3D12_INDEX_BUFFER_VIEW> ibViews_;
    std::unordered_map<uint32_t, uint32_t> indexCounts_;

    uint32_t nextId_ = 1;
};
