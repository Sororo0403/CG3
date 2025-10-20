#include "Mesh.h"
#include "BufferUtility.h"
#include <cassert>
#include <array>
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

    // Upload バッファ生成 → 一括書き込み
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

void Mesh::CreateBox(ID3D12Device *device, float sx, float sy, float sz, bool /*ccw_unused*/) {
    assert(device);

    const float hx = sx * 0.5f, hy = sy * 0.5f, hz = sz * 0.5f;

    std::vector<Vertex>   v; v.reserve(24);
    std::vector<uint16_t> i; i.reserve(36);

    auto push_tri = [&](uint16_t b, uint16_t i0, uint16_t i1, uint16_t i2,
        const DirectX::XMFLOAT3 &n) {
            // 反転判定： (p1 - p0) × (p2 - p0) と想定法線 n の向きが逆なら入れ替える
            using namespace DirectX;
            XMVECTOR p0 = XMLoadFloat3(&v[b + i0].pos);
            XMVECTOR p1 = XMLoadFloat3(&v[b + i1].pos);
            XMVECTOR p2 = XMLoadFloat3(&v[b + i2].pos);
            XMVECTOR c = XMVector3Cross(XMVectorSubtract(p1, p0), XMVectorSubtract(p2, p0));
            XMVECTOR nn = XMLoadFloat3(&n);
            float dot; XMStoreFloat(&dot, XMVector3Dot(c, nn));
            if (dot < 0.0f) std::swap(i1, i2); // 反転してCCWに合わせる
            i.push_back(b + i0); i.push_back(b + i1); i.push_back(b + i2);
        };

    auto addFace = [&](float nx, float ny, float nz,
        std::array<float, 3> p0, std::array<float, 3> p1,
        std::array<float, 3> p2, std::array<float, 3> p3) {
            const uint16_t base = static_cast<uint16_t>(v.size());
            DirectX::XMFLOAT3 n{nx, ny, nz};

            Vertex v0{{p0[0], p0[1], p0[2]}, n, {0.0f, 0.0f}};
            Vertex v1{{p1[0], p1[1], p1[2]}, n, {1.0f, 0.0f}};
            Vertex v2{{p2[0], p2[1], p2[2]}, n, {1.0f, 1.0f}};
            Vertex v3{{p3[0], p3[1], p3[2]}, n, {0.0f, 1.0f}};
            v.push_back(v0); v.push_back(v1); v.push_back(v2); v.push_back(v3);

            // 2三角形を自動でCCWに補正してから追加
            push_tri(base, 0, 1, 2, n);
            push_tri(base, 0, 2, 3, n);
        };

    // +X
    addFace(+1, 0, 0, {+hx,-hy,-hz}, {+hx,-hy,+hz}, {+hx,+hy,+hz}, {+hx,+hy,-hz});
    // -X
    addFace(-1, 0, 0, {-hx,-hy,+hz}, {-hx,-hy,-hz}, {-hx,+hy,-hz}, {-hx,+hy,+hz});
    // +Y (上)
    addFace(0, +1, 0, {-hx,+hy,-hz}, {+hx,+hy,-hz}, {+hx,+hy,+hz}, {-hx,+hy,+hz});
    // -Y (下)
    addFace(0, -1, 0, {-hx,-hy,+hz}, {+hx,-hy,+hz}, {+hx,-hy,-hz}, {-hx,-hy,-hz});
    // +Z (手前)
    addFace(0, 0, +1, {-hx,-hy,+hz}, {+hx,-hy,+hz}, {+hx,+hy,+hz}, {-hx,+hy,+hz});
    // -Z (奥)
    addFace(0, 0, -1, {+hx,-hy,-hz}, {-hx,-hy,-hz}, {-hx,+hy,-hz}, {+hx,+hy,-hz});

    UploadVertexData_(device, v.data(), v.size() * sizeof(Vertex), sizeof(Vertex));
    UploadIndexData16_(device, i.data(), i.size());
}
