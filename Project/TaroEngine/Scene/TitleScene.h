#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include "EngineContext.h"
#include "RenderContext.h"
#include "Camera.h"
#include "Model.h"
#include "IScene.h"

/// タイトルシーン：工事現場風の多層背景を描画（ゲームシーンと同じカメラ条件）
class TitleScene : public IScene {
public:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

private:
    // GameScene と同等の単独ロード版（WIC → SRV）
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    // モデル描画ヘルパ（★“幅・高さ・奥行き”→半サイズへ自動変換）
    void DrawModel_(Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg);

    // ウィンドウサイズに応じて正射影サイズを更新
    void RefreshCameraOrtho_();

private:
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;
    Camera                camera_;

    // 使用モデル（Block フォルダにある既存資産のみ）
    Model mdlSolid_;        // 濃グレー（背景・梁・板などベース）
    Model mdlJumpOnly_;     // 明るいグレー（ガーニッシュや手すり）
    Model mdlSpike_;        // 青系（ライトコーン等）
    Model mdlSpring_;       // 水色（雲・板）
    Model mdlSwitch_;       // 黄色（注意色・投光器）
    Model mdlSwitchOn_;     // さらに明るい黄色（光源・警告灯）
    Model mdlSwitchOff_;    // 中間色（パーツ分け用）

    // SRV（ComPtr任せで破棄）
    Microsoft::WRL::ComPtr<ID3D12Resource> texSolid_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texJumpOnly_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpike_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSpring_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitch_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOn_;
    Microsoft::WRL::ComPtr<ID3D12Resource> texSwitchOff_;

    // タイトル専用の SRV スロット（他と衝突しない帯域）
    enum : UINT {
        kSrv_T_Solid = 64,
        kSrv_T_JumpOnly,
        kSrv_T_Spike,
        kSrv_T_Spring,
        kSrv_T_Switch,
        kSrv_T_SwitchOn,
        kSrv_T_SwitchOff,
    };

    // 正射影の仮想高さ（幅はアスペクトから決定）
    float virtualWorldH_ = 22.0f;
};
