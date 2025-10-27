#pragma once
#define NOMINMAX

#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <cstdint>

#include "EngineContext.h"
#include "RenderContext.h"
#include "Camera.h"
#include "Model.h"
#include "IScene.h"
#include "BufferUtility.h"

// ステージセレクト画面。
// 背景は TitleScene と同じ工事現場の夜景。
// A/D でステージ番号を動かし、そのCSVをミニマップ的にプレビュー表示。
// Enter でGameScene(curStage_)に遷移する。
class StageSelectScene : public IScene {
public:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

private:
    // ===== 内部タイル定義（GameScene から必要なとこだけ持ってくる） =====
    static constexpr int   kMapW = 26;
    static constexpr int   kMapH = 15;
    static constexpr float kTile = 1.0f;

    enum class Tile : int32_t {
        Empty = 0,
        Solid,
        FragileAny,
        FragileTop,
        FragileBottom,
        Spring,
        Spike,
        JumpOnly,
        Regen,
        Switch,
        SwitchBlockOn,
        SwitchBlockOff,
    };

    struct FragileState {
        bool  gone = false;
    };

    struct PreviewCell {
        Tile t = Tile::Empty;
        bool gone = false; // fragile壊れてるかとか、ここはfalse固定でいい
    };

    // ====== 背景用パラメータ ======
    float virtualWorldH_ = 22.0f; // TitleSceneと同じ仮想高さ
    float blinkTime_ = 0.0f;
    float blinkStrength_ = 0.0f;

    // ====== 参照 ======
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;
    Camera camera_;

    // ====== モデル軍（TitleScene / GameScene と揃える） ======
    Model mdlSolid_;
    Model mdlFragileAny_;
    Model mdlFragileTop_;
    Model mdlFragileBottom_;
    Model mdlRegen_;
    Model mdlSpring_;
    Model mdlSpike_;
    Model mdlSwitch_;
    Model mdlSwitchOn_;
    Model mdlSwitchOff_;
    Model mdlJumpOnly_;

    // ====== Texture ComPtr（SRV存続用） ======
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

    enum : UINT {
        kSrv_T_Solid = 64,
        kSrv_T_FragileAny,
        kSrv_T_FragileTop,
        kSrv_T_FragileBottom,
        kSrv_T_Regen,
        kSrv_T_Spring,
        kSrv_T_Spike,
        kSrv_T_Switch,
        kSrv_T_SwitchOn,
        kSrv_T_SwitchOff,
        kSrv_T_JumpOnly,
    };

    // ステージ選択状態
    int curStage_ = 1;
    static constexpr int kMinStage_ = 1;
    static constexpr int kMaxStage_ = 30; // 仮の上限。必要なら変えて

    // プレビュー用グリッド
    Tile previewGrid_[kMapH][kMapW]{};
    bool switchOnPreview_ = false; // スイッチ系ブロックの表示用
    float xOffsetPreview_ = 0.0f;  // プレビューを中央寄せする

private:
    // 便利ユーティリティ
    static std::wstring Widen_(const std::string &u8);
    bool LoadTextureSRV_(const std::wstring &fileU16, UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle);

    void RefreshCameraOrtho_();
    void DrawModel_(Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg);

    void DrawBackgroundLayers_(float W, float H);
    void DrawStageNumberBanner_(float W, float H);
    void DrawPreviewMiniMap_(float W, float H);

    void LoadPreviewFromCSV_();
};
