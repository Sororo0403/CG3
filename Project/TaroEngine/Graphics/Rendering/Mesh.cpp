#include "Mesh.h"
#include "BufferUtility.h"
#include <cassert>
#include <cstring>
void Mesh::CreateBox(ID3D12Device *device) {
    std::vector<VertexPNUT> v;
    std::vector<uint16_t>   i;

    auto addFace = [&](float nx, float ny, float nz,
        float ax, float ay, float az,   // 右方向ベクトル(半径)
        float bx, float by, float bz,   // 上方向ベクトル(半径)
        float cx, float cy, float cz) { // 面の中心
            VertexPNUT quad[4];
            // 4隅（左下→右下→右上→左上）の順に生成
            float corners[4][3] = {
                {cx - ax - bx, cy - ay - by, cz - az - bz}, // v0: 左下
                {cx + ax - bx, cy + ay - by, cz + az - bz}, // v1: 右下
                {cx + ax + bx, cy + ay + by, cz + az + bz}, // v2: 右上
                {cx - ax + bx, cy - ay + by, cz - az + bz}, // v3: 左上
            };
            float t[4][2] = {{0,1},{1,1},{1,0},{0,0}}; // お好みで

            for (int k = 0; k < 4; ++k) {
                auto &vv = quad[k];
                vv.pos[0] = corners[k][0]; vv.pos[1] = corners[k][1]; vv.pos[2] = corners[k][2];
                vv.nrm[0] = nx; vv.nrm[1] = ny; vv.nrm[2] = nz;
                vv.uv[0] = t[k][0];       vv.uv[1] = t[k][1];
            }

            const uint16_t base = static_cast<uint16_t>(v.size());
            v.insert(v.end(), quad, quad + 4);

            // ★ここがポイント：CCW になるようにインデックスを組む
            // 三角1: v0(左下) -> v3(左上) -> v2(右上)
            // 三角2: v0(左下) -> v2(右上) -> v1(右下)
            uint16_t tris[6] = {base + 0u, base + 3u, base + 2u,
                                 base + 0u, base + 2u, base + 1u};
            i.insert(i.end(), tris, tris + 6);
        };

    // 半径ベクトル（±0.5 の箱）
    const float hx = 0.5f, hy = 0.5f, hz = 0.5f;

    // +Z 面（外側から見ると手前）。中心(0,0,+hz)、右=+X、上=+Y、法線+Z
    addFace(0, 0, +1, hx, 0, 0, 0, hy, 0, 0, 0, +hz);
    // -Z 面。中心(0,0,-hz)、右=+X、上=-Y、法線-Z（上ベクトルを反転させ CCW を維持）
    addFace(0, 0, -1, hx, 0, 0, 0, -hy, 0, 0, 0, -hz);
    // +X 面。中心(+hx,0,0)、右=+Z、上=+Y、法線+X
    addFace(+1, 0, 0, 0, 0, hz, 0, hy, 0, +hx, 0, 0);
    // -X 面。中心(-hx,0,0)、右=-Z、上=+Y、法線-X
    addFace(-1, 0, 0, 0, 0, -hz, 0, hy, 0, -hx, 0, 0);
    // +Y 面。中心(0,+hy,0)、右=+X、上=-Z、法線+Y
    addFace(0, +1, 0, hx, 0, 0, 0, 0, -hz, 0, +hy, 0);
    // -Y 面。中心(0,-hy,0)、右=+X、上=+Z、法線-Y
    addFace(0, -1, 0, hx, 0, 0, 0, 0, hz, 0, -hy, 0);

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
