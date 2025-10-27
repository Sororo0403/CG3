#pragma once
#include "IScene.h"
#include "Camera.h"
#include "Model.h"
#include "EngineContext.h"
#include "RenderContext.h"

class TitleScene : public IScene {
public:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Finalize() override;
    void Update(float dt) override;
    void Draw() override;

private:
    struct FallingBlock {
        bool   alive = false;
        DirectX::XMFLOAT3 pos{0,0,0};
        float  baseX = 0.0f;
        float  t = 0.0f;
        float  phase = 0.0f;

        float  fallSpeed = 6.0f;
        float  swayAmp = 1.0f;
        float  swayFreq = 1.0f;

        // ★固定サイズ（いじらない）
        float  w = 1.0f;
        float  h = 1.0f;
        float  d = 1.0f;

        int    kind = 0;
        float  rotZDeg = 0.0f;
    };

    // 内部関数
    void RefreshCameraOrtho_();
    void SpawnOne_();       // 新しいブロックを1つ出す
    void UpdateDebris_(float dt);
    void DrawDebris_();
    void DrawBackground_();
    void DrawModel_(Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg,
        float alphaMul = 1.0f);
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);
    static float Hash01_(int seed);

private:
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;

    Camera camera_;

    // シーン全体の見える範囲
    float worldW_ = 32.0f;
    float worldH_ = 18.0f;

    // スポーン領域
    float spawnTopY_ = 16.0f; // 画面上のちょい外
    float despawnY_ = -4.0f; // これより下に落ちたら消す
    float spawnLeftX_ = -8.0f;
    float spawnRightX_ = 8.0f;

    // 落ちているブロックたち
    static constexpr int kMaxBlocks_ = 32;
    FallingBlock blocks_[kMaxBlocks_];

    // スポーン間隔
    float spawnTimer_ = 0.0f;
    float spawnInterval_ = 0.1f; // 2秒に1個

    // モデル
    Model mdlSolid_;
    Model mdlFragileAny_;
    Model mdlFragileTop_;
    Model mdlFragileBottom_;
    Model mdlRegen_;
    Model mdlSpring_;
    Model mdlSpike_;
    Model mdlSwitch_;
    Model mdlSwitchBlockOn_;
    Model mdlSwitchBlockOff_;
    Model mdlJumpOnly_;

    int nextKindIndex_ = 0; // 0..10を回す


    // テクスチャ保持(SRV用にリソース持っておく)
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileTop_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileBottom_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texRegen_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;

};
