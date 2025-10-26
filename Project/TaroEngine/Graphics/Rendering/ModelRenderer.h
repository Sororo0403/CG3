#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <cstdint>
#include "Transform.h"
#include "Model.h"

class ShaderCompiler;
class Camera; // 前方宣言だけでOK

/// <summary>
/// 3Dモデル描画の基本レンダラ。
/// View/Proj を毎フレーム受け取り、ModelごとのWorld行列を更新して描画。
/// テクスチャ(map_Kd)にも対応（t0 & s0）。
/// </summary>
class ModelRenderer {
public:
    void Initialize(ID3D12Device *device, ShaderCompiler *shader);
    void Finalize() noexcept;

    /// <summary>
    /// ビュー行列・射影行列を渡して描画を開始します。
    /// </summary>
    void Begin(ID3D12GraphicsCommandList *commandList,
        const DirectX::XMMATRIX &view,
        const DirectX::XMMATRIX &proj) noexcept;

    /// <summary>
    /// Camera を直接渡して Begin を呼べる糖衣構文。
    /// </summary>
    void Begin(ID3D12GraphicsCommandList *commandList,
        const Camera &camera) noexcept;

    /// <summary>
    /// 描画終了。
    /// </summary>
    void End(ID3D12GraphicsCommandList *commandList) noexcept;

    /// <summary>
    /// 指定したモデルを現在のビュー/射影設定で描画。
    /// </summary>
    void Draw(ID3D12GraphicsCommandList *commandList,
        const Model &model, const Transform &transform) noexcept;

private:
    void CreateRootSignature();
    void CreatePipelineState();

private:
    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;
    ShaderCompiler *shader_ = nullptr;

    // ObjectCB リングバッファ管理
    static constexpr uint32_t kMaxObjectsPerFrame = 4096;
    uint32_t objectCBStride_ = 0; // AlignCB(sizeof(ObjectCB))
    uint32_t objectCBTotalBytes_ = 0; // objectCBStride_ * kMaxObjectsPerFrame
    uint32_t objectCBOffset_ = 0; // 現在の書き込みカーソル

    // === 定数バッファ構造 ===
    struct alignas(16) SceneCB {
        DirectX::XMFLOAT4X4 view; // 転置済み
        DirectX::XMFLOAT4X4 proj; // 転置済み
    };
    struct alignas(16) ObjectCB {
        DirectX::XMFLOAT4X4 world; // 転置済み
        float color[4];            // 予備（ティント等）
        uint32_t useTexture;       // 0=頂点色(Kd/d), 1=テクスチャ
        uint32_t _pad_[3];         // 16Bアライン調整
    };
};
