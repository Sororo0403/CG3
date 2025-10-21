#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include "Transform.h"

class Model;
class ShaderCompiler;

/// <summary>
/// 単純なモデル描画器（シーンCB + オブジェクトCB）
/// - ObjectCB は 256B 整列のリングバッファ（1フレーム中に複数 Draw 可）
/// </summary>
class ModelRenderer {
public:
    /// <summary>
    /// 初期化（デバイスとシェーダコンパイラを受け取る）
    /// </summary>
    void Initialize(ID3D12Device *device, ShaderCompiler *shader);

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() noexcept;

    /// <summary>
    /// フレーム先頭。VP を設定して PSO/RootSig をバインド
    /// </summary>
    void Begin(ID3D12GraphicsCommandList *commandList, const float view[16], const float proj[16]) noexcept;

    /// <summary>
    /// フレーム末尾（現在は何もしない）
    /// </summary>
    void End(ID3D12GraphicsCommandList *commandList) noexcept;

    /// <summary>
    /// 1 つのモデルを描画
    /// </summary>
    void Draw(ID3D12GraphicsCommandList *commandList, const Model &model, const Transform &transform) noexcept;

private:
    void CreateRootSignature();
    void CreatePipelineState();

private:
    Microsoft::WRL::ComPtr<ID3D12Device>         device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature>  rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState>  pso_;

    struct SceneCB { float view[16]; float proj[16]; };
    struct ObjectCB { float world[16]; float color[4]; }; // sizeof= (16+4)*16 = 320B ではない。float=4Bなので 16*4*? 注意: 実配列なので後述の256B整列で送る

    // ====== 定数バッファ ======
    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;

    // Map 先頭ポインタ
    SceneCB *sceneCBMapped_ = nullptr;
    uint8_t *objectCBMapped_ = nullptr; // リングの先頭（生ポインタ）

    // ====== リングバッファ制御 ======
    // 256B 整列サイズ（ハード要件）
    static constexpr uint32_t kObjectCBStride = ((sizeof(ObjectCB) + 255u) & ~255u);
    // 1 フレーム内で最大この回数の Draw を想定（必要に応じて増やしてください）
    static constexpr uint32_t kMaxObjectsPerFrame = 2048;
    // 総バイト数
    static constexpr uint32_t kObjectCBTotalBytes = kObjectCBStride * kMaxObjectsPerFrame;

    // 現フレームの書き込みオフセット
    uint32_t objectCBOffset_ = 0;

    ShaderCompiler *shader_ = nullptr;
};
