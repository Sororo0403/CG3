#pragma once
#include "IScene.h"
#include "Camera.h"
#include "Model.h"
#include "EngineContext.h"
#include "RenderContext.h"

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

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

        // ブロックは常に等倍（ゲーム内タイル感と揃える）
        float  w = 1.0f;
        float  h = 1.0f;
        float  d = 1.0f;

        int    kind = 0;       // どのモデルを使うか（0～10）
        float  rotZDeg = 0.0f; // いまは固定0で回転させない
    };

    // 内部処理
    void RefreshCameraOrtho_();
    void SpawnOne_();                // 新しいブロックを1つ出す
    void UpdateDebris_(float dt);    // 物理っぽい落下アニメ更新
    void DrawDebris_();              // 全ブロック描画
    void DrawModel_(
        Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg,
        float alphaMul = 1.0f
    );

    bool LoadTextureSRV_(
        const std::wstring &fileU16,
        UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle
    );

    static float Hash01_(int seed);

private:
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;

    Camera camera_;

    // 画面に見せたいワールド範囲（タイトル演出用の「ステージサイズ」）
    float worldW_ = 32.0f;
    float worldH_ = 18.0f;

    // 落下スポーンと消滅ライン
    float spawnTopY_ = 16.0f;   // 画面ちょい上から出す
    float despawnY_ = -4.0f;   // ここより下に落ちたら消す
    float spawnLeftX_ = -8.0f;
    float spawnRightX_ = 8.0f;

    // 最大この数だけ同時に落とす
    static constexpr int kMaxBlocks_ = 32;
    FallingBlock blocks_[kMaxBlocks_];

    // スポーン制御
    float spawnTimer_ = 0.0f;
    float spawnInterval_ = 0.1f;  // 0.1秒おきくらいでポロポロ落ちる
    int   nextKindIndex_ = 0;     // 0..10 を順番にローテする

    // モデル郡
    Model mdlSolid_;
    Model mdlFragileAny_;
    Model mdlFragileTop_;
    Model mdlFragileBottom_;
    Model mdlRegen_;
    Model mdlSpring_;
    Model mdlSpike_;

    // スイッチ本体：ONモデル / OFFモデル
    Model mdlSwitchOn_;
    Model mdlSwitchOff_;

    // スイッチ連動床：ON時だけ出る床 / OFF時だけ出る床
    Model mdlSwitchBlockOn_;
    Model mdlSwitchBlockOff_;

    Model mdlJumpOnly_;

    // テクスチャ保持（SRVリソース）
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
};
