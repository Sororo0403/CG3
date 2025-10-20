#include "Mesh.h"
#include "BufferUtility.h"
#include <cassert>
#include <vector>

using Microsoft::WRL::ComPtr;

void Mesh::Reset() noexcept {
    vb_.Reset();
    ib_.Reset();
    vbv_ = {};
    ibv_ = {};
    indexCount_ = 0;
}

void Mesh::UploadVertexData_(ID3D12Device *device, const void *data, size_t bytes, UINT stride) {
    Reset(); // VB/IB 再作成時はリセット

    vb_ = BufferUtility::CreateUploadBuffer(device, static_cast<uint64_t>(bytes));
    BufferUtility::WriteToUpload(vb_.Get(), data, bytes);

    vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = stride;
    vbv_.SizeInBytes = static_cast<UINT>(bytes);
}

void Mesh::UploadIndexData16_(ID3D12Device *device, const uint16_t *data, size_t count) {
    const size_t bytes = count * sizeof(uint16_t);

    ib_ = BufferUtility::CreateUploadBuffer(device, static_cast<uint64_t>(bytes));
    BufferUtility::WriteToUpload(ib_.Get(), data, bytes);

    ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibv_.Format = DXGI_FORMAT_R16_UINT;
    ibv_.SizeInBytes = static_cast<UINT>(bytes);

    indexCount_ = static_cast<UINT>(count);
}

void Mesh::UploadIndexData32_(ID3D12Device *device, const uint32_t *data, size_t count) {
    const size_t bytes = count * sizeof(uint32_t);

    ib_ = BufferUtility::CreateUploadBuffer(device, static_cast<uint64_t>(bytes));
    BufferUtility::WriteToUpload(ib_.Get(), data, bytes);

    ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibv_.Format = DXGI_FORMAT_R32_UINT;
    ibv_.SizeInBytes = static_cast<UINT>(bytes);

    indexCount_ = static_cast<UINT>(count);
}

void Mesh::CreateFromVertices(
    ID3D12Device *device,
    const std::vector<Vertex> &vertices,
    const std::vector<uint32_t> &indices32_or_empty_for16) {
    assert(device);
    assert(!vertices.empty());

    UploadVertexData_(device, vertices.data(),
        vertices.size() * sizeof(Vertex),
        static_cast<UINT>(sizeof(Vertex)));

    if (!indices32_or_empty_for16.empty()) {
        UploadIndexData32_(device, indices32_or_empty_for16.data(),
            indices32_or_empty_for16.size());
    } else {
        const size_t vcount = vertices.size();
        assert(vcount <= 0xFFFF && "Too many vertices for 16-bit implicit indices");
        std::vector<uint16_t> idx(static_cast<size_t>(vcount));
        for (size_t i = 0; i < vcount; ++i) idx[i] = static_cast<uint16_t>(i);
        UploadIndexData16_(device, idx.data(), idx.size());
    }
}
