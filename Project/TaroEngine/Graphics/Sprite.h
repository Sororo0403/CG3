#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Material.h"
#include "TransformMatrix.h"
#include "VertexData.h"

/// <summary>
/// 2D スプライトを表すクラス。<br/>
/// 頂点バッファ・インデックスバッファ・マテリアル・変換行列を持ち、
/// 初期化、更新、描画を管理する。
/// </summary>
class Sprite {
public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    Sprite() = default;

    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~Sprite() = default;

    /// <summary>
    /// 初期化処理。<br/>
    /// 頂点・インデックス・マテリアル・変換行列バッファを生成し、CPUからアクセスできるようにマップする。
    /// </summary>
    /// <param name="device">D3D12 デバイス。</param>
    void Initialize(ID3D12Device *device);

    /// <summary>
    /// 毎フレームの更新処理。<br/>
    /// 位置・回転・サイズなどの変換パラメータを行列に反映する。
    /// </summary>
    void Update();

    /// <summary>
    /// 描画処理。<br/>
    /// コマンドリストに頂点/インデックスバッファをセットし、ドローコールを発行する。
    /// </summary>
    /// <param name="cmdList">描画先のコマンドリスト。</param>
    void Draw(ID3D12GraphicsCommandList *cmdList);

private:
    // GPU リソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;    // 頂点バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;     // インデックスバッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;  // マテリアル用定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_; // 変換行列用定数バッファ

    // バッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{}; // 頂点バッファビュー
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};  // インデックスバッファビュー

    // CPU からアクセスするためのポインタ
    VertexData *vertexData_ = nullptr;          // 頂点データ
    uint32_t *indexData_ = nullptr;             // インデックスデータ
    Material *materialData_ = nullptr;          // マテリアルデータ
    TransformMatrix *transformMatrixData_ = nullptr; // 変換行列データ

    // スプライトの変換パラメータ
    Vector2 position_ = {0.0f, 0.0f}; // 座標
    Vector2 size_ = {100.0f, 100.0f}; // サイズ（幅・高さ）
    float rotation_ = 0.0f;           // 回転角度（ラジアン）
};
