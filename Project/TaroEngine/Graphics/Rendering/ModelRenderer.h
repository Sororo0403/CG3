#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <array>

class Mesh;

class ModelRenderer {
public:
    // シングルトンにしない共通ハブ（必要なら EngineContext に入れて共有）
    void Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap = nullptr);
    void Finalize() noexcept;

    // フレーム毎の共通定数（ビュー・プロジェクションなど）をセット
    void Begin(ID3D12GraphicsCommandList *cmd,
        const float view[16], const float proj[16]) noexcept;
    void End(ID3D12GraphicsCommandList *cmd) noexcept;

    // 単体描画
    void Draw(ID3D12GraphicsCommandList *cmd,
        const Mesh &mesh,
        const float world[16],
        const float color[4] = kWhite) const noexcept;

    // デバイス喪失/リサイズでは再初期化不要（PSO/RSV は解像度非依存）
    static constexpr float kWhite[4] = {1,1,1,1};

private:
    // 内部：ルート/PSO作成
    void CreateRootSignature();
    void CreatePipelineState();

    struct SceneCB {
        float view[16];
        float proj[16];
    };
    struct ObjectCB {
        float world[16];
        float color[4];
    };

    Microsoft::WRL::ComPtr<ID3D12Device> device_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSig_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;

    // フレームと独立したアップロードCB（シンプル運用）
    Microsoft::WRL::ComPtr<ID3D12Resource> sceneCB_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectCB_;
    SceneCB *sceneCBMapped_ = nullptr;
    ObjectCB *objectCBMapped_ = nullptr;
};
