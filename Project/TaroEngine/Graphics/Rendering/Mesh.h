#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <cstdint>
#include "Vertex.h"

class Mesh {
public:
    // 頂点配列とインデックス(32bit)。indices が空なら 0..N-1 の16bitインデックスとして扱う。
    void CreateFromVertices(ID3D12Device *device,
        const std::vector<Vertex> &vertices,
        const std::vector<uint32_t> &indices32_or_empty_for16);

    void Reset() noexcept;

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return vbv_; }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return ibv_; }
    UINT GetIndexCount() const noexcept { return indexCount_; }

private:
    void UploadVertexData_(ID3D12Device *device,
        const void *data, size_t bytes, UINT stride);
    void UploadIndexData16_(ID3D12Device *device,
        const uint16_t *data, size_t count);
    void UploadIndexData32_(ID3D12Device *device,
        const uint32_t *data, size_t count);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
    D3D12_VERTEX_BUFFER_VIEW vbv_{};
    D3D12_INDEX_BUFFER_VIEW  ibv_{};
    UINT indexCount_ = 0;
};
