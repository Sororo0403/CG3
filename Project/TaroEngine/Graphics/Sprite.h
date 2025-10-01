#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Material.h"
#include "TransformMatrix.h"
#include "VertexData.h"
#include "Vector2.h"

class Camera; // ★ 前方宣言

/// <summary>
/// 2D スプライトを表すクラス。<br/>
/// 頂点バッファ・インデックスバッファ・マテリアル・変換行列を持ち、
/// 初期化、更新、描画を管理する。
/// </summary>
class Sprite {
public:
    /// <summary>コンストラクタ。</summary>
    Sprite() = default;
    /// <summary>デストラクタ。</summary>
    ~Sprite() = default;

    /// <summary>
    /// 初期化処理。頂点/インデックス/マテリアル/変換行列バッファを生成してマップする。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    void Initialize(ID3D12Device *device);

    /// <summary>
    /// 更新処理（互換用）。固定の正射影(1280x720)で WVP を組む。
    /// </summary>
    void Update();

    /// <summary>
    /// 更新処理（カメラ使用）。WVP = World * (View*Proj)。
    /// </summary>
    /// <param name="camera">3D カメラ。</param>
    void Update(const Camera &camera);

    /// <summary>描画（バッファとCBをセットしてドロー）。</summary>
    /// <param name="cmdList">描画先のコマンドリスト。</param>
    void Draw(ID3D12GraphicsCommandList *cmdList);

    // --- パラメータ操作 ---
    void SetPosition(const Vector2 &p) { position_ = p; }
    void SetSize(const Vector2 &s) { size_ = s; }
    void SetRotation(float r) { rotation_ = r; }

private:
    // GPU リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;

    // ビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

    // マップ先
    VertexData *vertexData_ = nullptr;
    uint32_t *indexData_ = nullptr;
    Material *materialData_ = nullptr;
    TransformMatrix *transformMatrixData_ = nullptr;

    // 変換パラメータ
    Vector2 position_{0.0f, 0.0f};
    Vector2 size_{100.0f, 100.0f};
    float   rotation_ = 0.0f;

    // 内部共通：頂点更新＋World 計算を行い、引数 vp（= View*Proj）で WVP を組む
    void UpdateImpl_(const struct Matrix4x4 &vp);
};
