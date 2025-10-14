#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include "Object3dCommon.h"   // ルートシグネチャ/PSO を持つ共通部
#include "DirectXCommon.h"    // Device, CmdList, Heaps 取得に使用
#include "DirectXTex/d3dx12.h"

struct ObjectVertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

struct MaterialCB {
    float color[4];       // RGBA
    int   useTexture;     // 0/1
    float pad[3];
    float uvTransform[16];// 行列(列優先/行優先はシェーダに合わせて)
};

struct TransformCB {
    float world[16];
    float viewProj[16];
    float cameraPos[3];
    float _pad0;
};

struct DirectionalLightCB {
    float color[3];  float intensity;
    float direction[3]; float _pad1;
};

/// <summary>
/// .obj + .png を1枚想定した最小の3Dオブジェクト
/// </summary>
class Object3d {
public:
    void Initialize(Object3dCommon *common);      // 共通部を注入
    bool LoadObj(const std::wstring &objPath);    // 頂点/インデックス作成
    void SetTextureSrv(D3D12_GPU_DESCRIPTOR_HANDLE gpuSrv); // 外部で作ったSRVを設定

    // 変換・マテリアル・ライトの編集用
    void SetPosition(float x, float y, float z);
    void SetRotation(float rx, float ry, float rz); // radians
    void SetScale(float sx, float sy, float sz);
    void SetColor(float r, float g, float b, float a);
    void SetDirLight(const float dir[3], const float col[3], float intensity);

    // 毎フレーム
    void Update(const float viewProj[16], const float cameraPos[3]);
    void Draw(ID3D12GraphicsCommandList *cmdList);

private:
    void CreateVertexBuffer(const std::vector<ObjectVertex> &verts);
    void CreateIndexBuffer(const std::vector<uint32_t> &indices);
    void CreateCBs();

private:
    Object3dCommon *common_ = nullptr;

    // VB/IB
    Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
    D3D12_INDEX_BUFFER_VIEW  ibView_{};
    uint32_t indexCount_ = 0;

    // CBs
    Microsoft::WRL::ComPtr<ID3D12Resource> materialCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> lightCB_;
    MaterialCB   material_{{1,1,1,1}, 0, {0,0,0}, {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}};
    TransformCB  transform_{};
    DirectionalLightCB light_{{1,1,1}, 1.0f, {0,-1,0}, 0};

    // SRV（テクスチャは外から差し込む想定）
    D3D12_GPU_DESCRIPTOR_HANDLE texSrv_{};  // .ptr==0 のとき未設定

    // Transform（右手前提の最小実装）
    float pos_[3]{0,0,0};
    float rot_[3]{0,0,0};
    float scale_[3]{1,1,1};
};
