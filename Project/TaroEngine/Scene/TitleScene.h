#pragma once
#define NOMINMAX

#include "IScene.h"
#include "Camera.h"
#include "Model.h"
#include "EngineContext.h"
#include "RenderContext.h"

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>

class TitleScene : public IScene {
public:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Finalize() override;
    void Update(float dt) override;
    void Draw() override;

private:
    // ===== 参照 =====
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;

    Camera camera_;

    // ===== 背景や雨のブロックに使うモデル =====
    Model mdlSolid_;
    Model mdlFragileAny_;
    Model mdlFragileTop_;
    Model mdlFragileBottom_;
    Model mdlRegen_;
    Model mdlSpring_;
    Model mdlSpike_;
    Model mdlSwitchOn_;
    Model mdlSwitchOff_;
    Model mdlSwitchBlockOn_;
    Model mdlSwitchBlockOff_;
    Model mdlJumpOnly_;

    // タイトルロゴ用
    Model mdlTitleLogo_;

    // テクスチャリソース保持（参照が死なないように）
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileTop_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileBottom_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texRegen_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchBlockOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texTitleLogo_;

    // ===== 画面内ワールド範囲 =====
    float worldW_ = 32.0f;
    float worldH_ = 18.0f;

    // ===== 落下ブロック（雨エフェクト） =====
    struct FallingBlock {
        bool   alive = false;
        DirectX::XMFLOAT3 pos{0,0,0};

        float  baseX = 0.0f;
        float  t = 0.0f;
        float  phase = 0.0f;

        float  fallSpeed = 6.0f;
        float  swayAmp = 1.0f;
        float  swayFreq = 1.0f;

        float  w = 1.0f;
        float  h = 1.0f;
        float  d = 1.0f;

        int    kind = 0;
        float  rotZDeg = 0.0f;
    };

    static constexpr int kMaxBlocks_ = 128;
    FallingBlock blocks_[kMaxBlocks_];

    float spawnTopY_ = 0.0f;
    float despawnY_ = 0.0f;
    float spawnLeftX_ = 0.0f;
    float spawnRightX_ = 0.0f;

    float spawnTimer_ = 0.0f;
    float spawnInterval_ = 0.1f;
    int   nextKindIndex_ = 0;

    // ==== 内部関数 ====
    static float Hash01_(int seed);

    void RefreshCameraOrtho_();
    void SpawnOne_();
    void UpdateDebris_(float dt);
    void DrawDebris_();

    void DrawModel_(
        Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg,
        float alphaMul);

    bool LoadTextureSRV_(
        const std::wstring &fileU16,
        UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle
    );

    // 背景を組み立てて描画
    void DrawTitleBackground_();

    // タイトルロゴを中央に描画
    void DrawTitleLogo_();
};
