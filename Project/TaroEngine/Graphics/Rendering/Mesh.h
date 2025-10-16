#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

struct VertexPNUT {
    float pos[3];
    float nrm[3];
    float uv[2];
};

class Mesh {
public:
    // とりあえず箱（単位キューブ）を生成
    void CreateBox(ID3D12Device *device);

    const D3D12_VERTEX_BUFFER_VIEW &GetVBV() const noexcept { return vbv_; }
    const D3D12_INDEX_BUFFER_VIEW &GetIBV() const noexcept { return ibv_; }
    UINT GetIndexCount() const noexcept { return indexCount_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
    D3D12_VERTEX_BUFFER_VIEW vbv_{};
    D3D12_INDEX_BUFFER_VIEW  ibv_{};
    UINT indexCount_ = 0;
};
