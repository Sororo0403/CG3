#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "VertexData.h"
#include "Material.h"
#include "TransformMatrix.h"

class Sprite {
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(ID3D12Device *device);

    // 画面サイズ（ピクセル）※リサイズ時に呼ぶ
    void SetViewportSize(uint32_t w, uint32_t h);

    // スプライト矩形（ピクセル）
    void SetRect(float x, float y, float width, float height);

    // 色（Material.color）RGBA
    void SetColor(float r, float g, float b, float a);

    // 毎フレーム更新（行列やCBの更新だけ。カメラ不要）
    void Update();

    // 描画（共通PSO適用後に呼ぶ）
    void Draw(ID3D12GraphicsCommandList *cmd) const;

private:
    void CreateBuffers(ID3D12Device *device);
    void UpdateVertices();      // Rect 変更反映
    void UpdateConstants();     // WVP/Color をCBへ

    static TransformMatrix MakePixelToNDC(uint32_t vpw, uint32_t vph);

private:
    ID3D12Device *device_ = nullptr;

    // 頂点/インデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> vb_;
    Microsoft::WRL::ComPtr<ID3D12Resource> ib_;
    D3D12_VERTEX_BUFFER_VIEW vbv_{};
    D3D12_INDEX_BUFFER_VIEW  ibv_{};
    VertexData *mappedVB_ = nullptr;
    uint32_t *mappedIB_ = nullptr;

    // CB（Upload）
    Microsoft::WRL::ComPtr<ID3D12Resource> cbMaterial_;
    Microsoft::WRL::ComPtr<ID3D12Resource> cbTransform_;
    Material *mappedMaterial_ = nullptr;
    TransformMatrix *mappedTransform_ = nullptr;

    // 状態
    uint32_t viewportW_ = 1, viewportH_ = 1;
    float x_ = 0, y_ = 0, w_ = 100, h_ = 100; // ピクセル矩形
};
