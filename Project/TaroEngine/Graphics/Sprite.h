#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <memory>
#include "VertexData.h"
#include "Material.h"
#include "TransformMatrix.h"

class Texture2D; // 前方宣言

class Sprite {
public:
    Sprite() = default;
    ~Sprite() = default;

    void Initialize(ID3D12Device *device);

    void SetViewportSize(uint32_t w, uint32_t h);
    void SetRect(float x, float y, float width, float height);
    void SetColor(float r, float g, float b, float a);
    void Update();
    void Draw(ID3D12GraphicsCommandList *cmd) const;

    // 分離後：スプライトはテクスチャを参照で受け取る
    void SetTexture(const std::shared_ptr<Texture2D> &tex);

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

    // テクスチャ参照
    std::shared_ptr<Texture2D> texture_;

    // 状態
    uint32_t viewportW_ = 1, viewportH_ = 1;
    float x_ = 0, y_ = 0, w_ = 100, h_ = 100; // ピクセル矩形
};
