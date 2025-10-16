#include "Mesh.h"
#include "BufferUtility.h"
#include <cassert>
#include <cstring>

void Mesh::CreateBox(ID3D12Device *device) {
    // 単位立方体（-0.5～+0.5）・面ごとに法線
    std::vector<VertexPNUT> v;
    std::vector<uint16_t> i;

    auto addFace = [&](float nx, float ny, float nz,
        float ax, float ay, float az,
        float bx, float by, float bz,
        float cx, float cy, float cz) {
            VertexPNUT quad[4];
            float t[4][2] = {{0,1},{1,1},{1,0},{0,0}};
            float corners[4][3] = {
                {cx - ax - bx, cy - ay - by, cz - az - bz},
                {cx + ax - bx, cy + ay - by, cz + az - bz},
                {cx + ax + bx, cy + ay + by, cz + az + bz},
                {cx - ax + bx, cy - ay + by, cz - az + bz},
            };
            for (int k = 0; k < 4; ++k) {
                auto &vv = quad[k];
                vv.pos[0] = corners[k][0]; vv.pos[1] = corners[k][1]; vv.pos[2] = corners[k][2];
                vv.nrm[0] = nx; vv.nrm[1] = ny; vv.nrm[2] = nz;
                vv.uv[0] = t[k][0];       vv.uv[1] = t[k][1];
            }
            const uint16_t base = static_cast<uint16_t>(v.size());
            v.insert(v.end(), quad, quad + 4);
            uint16_t tris[6] = {base, uint16_t(base + 1), uint16_t(base + 2),
                                base, uint16_t(base + 2), uint16_t(base + 3)};
            i.insert(i.end(), tris, tris + 6);
        };

    // +Z / -Z / +X / -X / +Y / -Y
    addFace(0, 0, 1, 0.5f, 0, 0, 0, 0.5f, 0, 0, 0, 0.5f);
    addFace(0, 0, -1, 0.5f, 0, 0, 0, -0.5f, 0, 0, 0, -0.5f);
    addFace(1, 0, 0, 0, 0, 0.5f, 0, 0.5f, 0, 0.5f, 0, 0);
    addFace(-1, 0, 0, 0, 0, 0.5f, 0, -0.5f, 0, -0.5f, 0, 0);
    addFace(0, 1, 0, 0.5f, 0, 0, 0, 0, 0.5f, 0, 0.5f, 0);
    addFace(0, -1, 0, 0.5f, 0, 0, 0, 0, -0.5f, 0, -0.5f, 0);

    indexCount_ = static_cast<UINT>(i.size());

    // VB
    vb_ = BufferUtility::CreateUploadBuffer(device, v.size() * sizeof(VertexPNUT));
    BufferUtility::WriteToUpload(vb_.Get(), v.data(), v.size() * sizeof(VertexPNUT));

    vbv_.BufferLocation = vb_->GetGPUVirtualAddress();
    vbv_.StrideInBytes = sizeof(VertexPNUT);
    vbv_.SizeInBytes = static_cast<UINT>(v.size() * sizeof(VertexPNUT));

    // IB
    ib_ = BufferUtility::CreateUploadBuffer(device, i.size() * sizeof(uint16_t));
    BufferUtility::WriteToUpload(ib_.Get(), i.data(), i.size() * sizeof(uint16_t));

    ibv_.BufferLocation = ib_->GetGPUVirtualAddress();
    ibv_.Format = DXGI_FORMAT_R16_UINT;
    ibv_.SizeInBytes = static_cast<UINT>(i.size() * sizeof(uint16_t));
}
