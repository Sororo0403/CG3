#include "MeshManager.h"

#include <cassert>
#include <cstring>
#include <directx/d3dx12.h>

#include "OBJ/OBJLoader.h"

using Microsoft::WRL::ComPtr;

MeshManager::MeshManager(ID3D12Device *device, OBJLoader *objLoader)
    : device_(device), objLoader_(objLoader) {
    assert(device_);
    assert(objLoader_);
}

uint32_t MeshManager::Register(std::unique_ptr<Mesh> mesh) {
    assert(mesh);
    assert(!mesh->vertices.empty());
    assert(!mesh->indices.empty());

    uint32_t id = nextId_++;

    meshes_.emplace(id, std::move(mesh));
    CreateGpuBuffers(id, *meshes_[id]);

    return id;
}

uint32_t MeshManager::RegisterFromOBJ(const std::string &path) {
    auto mesh = objLoader_->Load(path);
    assert(mesh);
    return Register(std::move(mesh));
}

void MeshManager::CreateGpuBuffers(uint32_t id, const Mesh &mesh) {
    // =========================
    // Vertex Buffer
    // =========================
    UINT vbSize =
        static_cast<UINT>(sizeof(Mesh::Vertex) * mesh.vertices.size());

    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    ComPtr<ID3D12Resource> vb;
    HRESULT hr = device_->CreateCommittedResource(
        &heap, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&vb));
    assert(SUCCEEDED(hr));

    void *mapped = nullptr;
    vb->Map(0, nullptr, &mapped);
    std::memcpy(mapped, mesh.vertices.data(), vbSize);
    vb->Unmap(0, nullptr);

    D3D12_VERTEX_BUFFER_VIEW vbView{};
    vbView.BufferLocation = vb->GetGPUVirtualAddress();
    vbView.SizeInBytes = vbSize;
    vbView.StrideInBytes = sizeof(Mesh::Vertex);

    // =========================
    // Index Buffer
    // =========================
    UINT ibSize = static_cast<UINT>(sizeof(uint32_t) * mesh.indices.size());

    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibSize);

    ComPtr<ID3D12Resource> ib;
    hr = device_->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &ibDesc,
                                          D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr, IID_PPV_ARGS(&ib));
    assert(SUCCEEDED(hr));

    ib->Map(0, nullptr, &mapped);
    std::memcpy(mapped, mesh.indices.data(), ibSize);
    ib->Unmap(0, nullptr);

    D3D12_INDEX_BUFFER_VIEW ibView{};
    ibView.BufferLocation = ib->GetGPUVirtualAddress();
    ibView.SizeInBytes = ibSize;
    ibView.Format = DXGI_FORMAT_R32_UINT;

    // 保存
    vertexBuffers_[id] = vb;
    indexBuffers_[id] = ib;
    vbViews_[id] = vbView;
    ibViews_[id] = ibView;
    indexCounts_[id] = static_cast<uint32_t>(mesh.indices.size());
}

const Mesh *MeshManager::GetMesh(uint32_t id) const {
    auto it = meshes_.find(id);
    if (it == meshes_.end()) {
        return nullptr;
    }
    return it->second.get();
}

const D3D12_VERTEX_BUFFER_VIEW *MeshManager::GetVBView(uint32_t id) const {
    auto it = vbViews_.find(id);
    return (it != vbViews_.end()) ? &it->second : nullptr;
}

const D3D12_INDEX_BUFFER_VIEW *MeshManager::GetIBView(uint32_t id) const {
    auto it = ibViews_.find(id);
    return (it != ibViews_.end()) ? &it->second : nullptr;
}

uint32_t MeshManager::GetIndexCount(uint32_t id) const {
    auto it = indexCounts_.find(id);
    return (it != indexCounts_.end()) ? it->second : 0;
}
