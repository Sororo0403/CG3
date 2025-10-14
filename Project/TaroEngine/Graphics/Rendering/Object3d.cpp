#include "Object3d.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <DirectXMath.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace {
    inline UINT AlignCB(UINT s) { return (s + 255u) & ~255u; }

    void *MapWrite(ID3D12Resource *res) {
        void *p = nullptr; res->Map(0, nullptr, &p); return p;
    }
    void Unmap(ID3D12Resource *res) { res->Unmap(0, nullptr); }

    XMMATRIX MakeSRT(const float s[3], const float r[3], const float t[3]) {
        return XMMatrixScaling(s[0], s[1], s[2]) *
            XMMatrixRotationRollPitchYaw(r[0], r[1], r[2]) *
            XMMatrixTranslation(t[0], t[1], t[2]);
    }
}

// ===== Initialize =====
void Object3d::Initialize(Object3dCommon *common) {
    assert(common);
    common_ = common;
    CreateCBs();
}

// ===== Load .obj (ultra-minimal) =====
bool Object3d::LoadObj(const std::wstring &path) {
    // 超簡易 .obj：v/vt/vn と f v/t/n v/t/n v/t/n の三角形のみ対応
    std::ifstream ifs(path);
    if (!ifs) { return false; }

    std::vector<XMFLOAT3> positions;
    std::vector<XMFLOAT3> normals;
    std::vector<XMFLOAT2> uvs;
    std::vector<ObjectVertex> verts;
    std::vector<uint32_t> indices;

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if (tag == "v") {
            XMFLOAT3 p; ss >> p.x >> p.y >> p.z; positions.push_back(p);
        } else if (tag == "vt") {
            XMFLOAT2 uv; ss >> uv.x >> uv.y; uv.y = 1.0f - uv.y; uvs.push_back(uv);
        } else if (tag == "vn") {
            XMFLOAT3 n; ss >> n.x >> n.y >> n.z; normals.push_back(n);
        } else if (tag == "f") {
            // 三角形のみ
            for (int k = 0; k < 3; k++) {
                std::string vtn; ss >> vtn;
                int vi = 0, ti = 0, ni = 0;
                sscanf_s(vtn.c_str(), "%d/%d/%d", &vi, &ti, &ni);
                ObjectVertex v{};
                auto P = positions[vi - 1];
                auto N = normals[ni - 1];
                auto UV = uvs[ti - 1];
                v.pos[0] = P.x; v.pos[1] = P.y; v.pos[2] = P.z;
                v.normal[0] = N.x; v.normal[1] = N.y; v.normal[2] = N.z;
                v.uv[0] = UV.x; v.uv[1] = UV.y;
                verts.push_back(v);
                indices.push_back(static_cast<uint32_t>(indices.size()));
            }
        }
    }

    CreateVertexBuffer(verts);
    CreateIndexBuffer(indices);
    indexCount_ = static_cast<uint32_t>(indices.size());
    return indexCount_ > 0;
}

// ===== External texture SRV =====
void Object3d::SetTextureSrv(D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv) { texSrv_ = gpuSrv; }

// ===== Editable state =====
void Object3d::SetPosition(float x, float y, float z) { pos_[0] = x; pos_[1] = y; pos_[2] = z; }
void Object3d::SetRotation(float rx, float ry, float rz) { rot_[0] = rx; rot_[1] = ry; rot_[2] = rz; }
void Object3d::SetScale(float sx, float sy, float sz) { scale_[0] = sx; scale_[1] = sy; scale_[2] = sz; }
void Object3d::SetColor(float r, float g, float b, float a) { material_.color[0] = r; material_.color[1] = g; material_.color[2] = b; material_.color[3] = a; }
void Object3d::SetDirLight(const float dir[3], const float col[3], float intensity) {
    light_.direction[0] = dir[0]; light_.direction[1] = dir[1]; light_.direction[2] = dir[2];
    light_.color[0] = col[0]; light_.color[1] = col[1]; light_.color[2] = col[2];
    light_.intensity = intensity;
}

// ===== Update (write CBs) =====
void Object3d::Update(const float viewProj[16], const float cameraPos[3]) {
    // World
    auto W = MakeSRT(scale_, rot_, pos_);
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4 *>(transform_.world), XMMatrixTranspose(W));
    // ViewProj
    memcpy(transform_.viewProj, viewProj, sizeof(transform_.viewProj));
    // Camera
    transform_.cameraPos[0] = cameraPos[0];
    transform_.cameraPos[1] = cameraPos[1];
    transform_.cameraPos[2] = cameraPos[2];

    // マテリアル
    material_.useTexture = (texSrv_.ptr != 0) ? 1 : 0;

    // 書き込み
    memcpy(MapWrite(materialCB_.Get()), &material_, sizeof(MaterialCB));  Unmap(materialCB_.Get());
    memcpy(MapWrite(transformCB_.Get()), &transform_, sizeof(TransformCB)); Unmap(transformCB_.Get());
    memcpy(MapWrite(lightCB_.Get()), &light_, sizeof(DirectionalLightCB)); Unmap(lightCB_.Get());
}

// ===== Draw =====
void Object3d::Draw(ID3D12GraphicsCommandList *cmd) {
    // 共通設定（ルートシグネチャ/PSO/IAトポロジ）
    common_->SetCommonDrawSetting(cmd);
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // VB/IB
    cmd->IASetVertexBuffers(0, 1, &vbView_);
    cmd->IASetIndexBuffer(&ibView_);

    // RootParameters（想定：0=Transform CB, 1=Material CB, 2=Light CB, 3=SRV）
    cmd->SetGraphicsRootConstantBufferView(0, transformCB_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(1, materialCB_->GetGPUVirtualAddress());
    cmd->SetGraphicsRootConstantBufferView(2, lightCB_->GetGPUVirtualAddress());
    if (texSrv_.ptr != 0) {
        cmd->SetGraphicsRootDescriptorTable(3, texSrv_);
    }

    cmd->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
}

// ===== resources =====
void Object3d::CreateVertexBuffer(const std::vector<ObjectVertex> &verts) {
    auto *dev = common_->GetDxCommon()->GetDevice();
    const UINT size = static_cast<UINT>(verts.size() * sizeof(ObjectVertex));

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vb_));

    void *mapped = MapWrite(vb_.Get());
    memcpy(mapped, verts.data(), size);
    Unmap(vb_.Get());

    vbView_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbView_.StrideInBytes = sizeof(ObjectVertex);
    vbView_.SizeInBytes = size;
}

void Object3d::CreateIndexBuffer(const std::vector<uint32_t> &indices) {
    auto *dev = common_->GetDxCommon()->GetDevice();
    const UINT size = static_cast<UINT>(indices.size() * sizeof(uint32_t));

    CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC   desc = CD3DX12_RESOURCE_DESC::Buffer(size);
    dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&ib_));

    void *mapped = MapWrite(ib_.Get());
    memcpy(mapped, indices.data(), size);
    Unmap(ib_.Get());

    ibView_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibView_.Format = DXGI_FORMAT_R32_UINT;
    ibView_.SizeInBytes = size;
}

void Object3d::CreateCBs() {
    auto *dev = common_->GetDxCommon()->GetDevice();

    auto makeCB = [&](UINT bytes, ComPtr<ID3D12Resource> &out) {
        CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(AlignCB(bytes));
        dev->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&out));
        };
    makeCB(sizeof(MaterialCB), materialCB_);
    makeCB(sizeof(TransformCB), transformCB_);
    makeCB(sizeof(DirectionalLightCB), lightCB_);
}
