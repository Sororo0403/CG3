#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <cstdint>
#include <array>

#include "Transform.h"
#include "Model.h"

class ShaderCompiler;
class Camera;
class DirectXCommon;

/// <summary>
/// 3Dモデル描画用レンダラ。
/// ・SceneCB(view/proj)は毎フレーム1回
/// ・ObjectCB(world等)はDrawごと
/// ・ObjectCBはフレームごとに別のアップロードバッファを使って、GPUが参照中のメモリを次フレームで上書きしない
/// </summary>
class ModelRenderer {
public:
    void Initialize(ID3D12Device *device, ShaderCompiler *shader);
    void Finalize() noexcept;

    void Begin(ID3D12GraphicsCommandList *commandList,
        DirectXCommon *dx,
        const DirectX::XMMATRIX &view,
        const DirectX::XMMATRIX &proj) noexcept;

    void Begin(ID3D12GraphicsCommandList *commandList,
        DirectXCommon *dx,
        const Camera &camera) noexcept;

    void End(ID3D12GraphicsCommandList *commandList) noexcept;

    void Draw(ID3D12GraphicsCommandList *commandList,
        const Model &model, const Transform &transform) noexcept;

private:
    void CreateRootSignature();
    void CreatePipelineState();

private:
    // スワップチェインと同じだけ用意（トリプルバッファ）
    static constexpr uint32_t kFrameCount = 3;

    // 1フレームで描ける最大オブジェクト数
    static constexpr uint32_t kMaxObjectsPerFrame = 4096;

    struct alignas(16) SceneCB {
        DirectX::XMFLOAT4X4 view; // 転置済み
        DirectX::XMFLOAT4X4 proj; // 転置済み
    };
    struct alignas(16) ObjectCB {
        DirectX::XMFLOAT4X4 world; // 転置済み
        float color[4];            // 予備(ティント用など)
        uint32_t useTexture;       // 0=頂点色, 1=テクスチャ
        uint32_t _pad_[3];         // 16Bアライン調整
    };

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    ShaderCompiler *shader_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

    // SceneCB は1本（毎フレーム上書きでOK）
    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;

    // ObjectCB はフレームごとに専用バッファを用意
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kFrameCount> objectCBs_{};

    // CBサイズ情報
    uint32_t objectCBStride_ = 0; // 256Bアライン済みサイズ
    uint32_t objectCBTotalBytes_ = 0; // stride * kMaxObjectsPerFrame

    // 現在のフレームインデックス (0..kFrameCount-1)
    uint32_t currentFrameIndex_ = 0;

    // フレームごとの書き込みカーソル
    uint32_t objectCBOffset_[kFrameCount] = {};
};
