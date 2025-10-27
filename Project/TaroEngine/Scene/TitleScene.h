#pragma once
#define NOMINMAX

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>

#include "EngineContext.h"
#include "RenderContext.h"
#include "Camera.h"
#include "Model.h"
#include "IScene.h"

/// <summary>
/// タイトルシーン：工事現場風の多層背景を描画し、
/// 画面中央に"SPACE"の看板をブロックで組んで点滅（擬似）表示する。
/// Spaceキーでステージセレクトに遷移する。
/// </summary>
class TitleScene : public IScene {
public:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

    /// <summary>
    /// モデル描画ヘルパ
    /// fullScale は「最終的な幅・高さ・奥行き」をそのまま指定すると、
    /// 内部で半分にしてScaleに入れる（=OBJが±1ベースのとき扱いやすい）
    /// </summary>
    void DrawModel_(Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg);

private:
    // GameScene と同等の単独ロード版（WIC → SRV）
    // テクスチャをロードし、指定のSRVヒープスロットにSRVを作成して
    // GPUハンドルをモデルへ渡す
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    // ウィンドウサイズに応じて正射影サイズを更新
    void RefreshCameraOrtho_();

    // "SPACE" のサインを描画する（組みブロック）
    // baseW/baseH は1文字あたりの見た目スケール
    void DrawSpaceSign_(float baseW, float baseH);

private:
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;
    Camera                camera_;

    // ====== 点滅制御 ======
    // 経過時間
    float blinkTime_ = 0.0f;
    // 0～1くらいで上下する「光の強さ」
    // Updateで更新して、DrawSpaceSign_で使う
    float blinkStrength_ = 0.0f;

    // ====== 使用モデル（既存Block系） ======
    Model mdlSolid_;         // 濃グレー：足場ベース
    Model mdlFragileAny_;    // ひび割れ/注意系
    Model mdlJumpOnly_;      // 明るいグレー(手すり/ケーブル)
    Model mdlSpike_;         // 青系：ライト照射コーン
    Model mdlSpring_;        // 水色：空や雲っぽい大板
    Model mdlSwitch_;        // 黄：機材
    Model mdlSwitchOn_;      // 明るい黄：発光ライト
    Model mdlSwitchOff_;     // 消灯／中間色

    // ====== SRVのリソース保持 ======
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texFragileAny_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;

    // ====== タイトル専用の SRV スロット（他のシーンと被らない帯域にしておく） ======
    enum : UINT {
        kSrv_T_Solid = 64,
        kSrv_T_FragileAny,
        kSrv_T_JumpOnly,
        kSrv_T_Spike,
        kSrv_T_Spring,
        kSrv_T_Switch,
        kSrv_T_SwitchOn,
        kSrv_T_SwitchOff,
    };

    // ====== 正射影の仮想高さ（幅はアスペクトから決定） ======
    float virtualWorldH_ = 22.0f;
};
