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
#include "Difficulty.h"

// ステージセレクト画面。
// A/D でステージ番号を動かす。
// W/S で難易度を切り替える。（★NEW）
// Space で GameScene(curStage_, curDifficulty_) に遷移。（★NEW）
class StageSelectScene : public IScene {
public:
    StageSelectScene(int startStage = 1,
        Difficulty startDiff = Difficulty::Normal)
        : startStage_(startStage)
        , startDiff_(startDiff) {
    }

    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

    const char *DiffToText_(Difficulty d);

private:
    // ===== GameSceneと揃えるステージ情報 =====
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

    struct PreviewCell {
        Tile  t = Tile::Empty;
        bool  gone = false;
    };

    // ===== 背景演出パラメータ =====
    float virtualWorldH_ = 22.0f;
    float blinkTime_ = 0.0f;
    float blinkStrength_ = 0.0f;

    // ===== 参照 =====
    const EngineContext *engine_ = nullptr;
    const RenderContext *render_ = nullptr;
    Camera camera_;

    // ===== モデル =====
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

    // ===== テクスチャ保持 =====
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

    enum : UINT {
        kSrv_T_Solid = 64,
        kSrv_T_FragileAny,
        kSrv_T_FragileTop,
        kSrv_T_FragileBottom,
        kSrv_T_Regen,
        kSrv_T_Spring,
        kSrv_T_Spike,
        kSrv_T_SwitchOn,
        kSrv_T_SwitchOff,
        kSrv_T_SwitchBlockOn,
        kSrv_T_SwitchBlockOff,
        kSrv_T_JumpOnly,
    };

    // ステージ番号
    int startStage_ = 1;
    int curStage_ = 1;
    static constexpr int kMinStage_ = 1;
    static constexpr int kMaxStage_ = 30; // 仮

    // ★NEW 難易度
    Difficulty startDiff_ = Difficulty::Normal;
    Difficulty curDiff_ = Difficulty::Normal;

    // プレビュー用グリッド
    Tile  previewGrid_[kMapH][kMapW]{};
    float xOffsetPreview_ = 0.0f;

private:
    // ユーティリティ
    static std::wstring Widen_(const std::string &u8);

    bool LoadTextureSRV_(
        const std::wstring &fileU16,
        UINT srvIndex,
        Microsoft::WRL::ComPtr<ID3D12Resource> &outTex,
        D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle
    );

    void RefreshCameraOrtho_();

    void DrawModel_(
        Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg
    );

    void DrawBackgroundLayers_(float W, float H);
    void DrawStageNumberBanner_(float W, float H);
    void DrawDifficultyBanner_(float W, float H); // ★NEW
    void DrawPreviewMiniMap_(float W, float H);

    void LoadPreviewFromCSV_();

    // ★NEW 難易度→文字列("easy","normal","hard")を返す
    static const char *DiffTag_(Difficulty d);

    // ★NEW 難易度バナー用のプレーンな英語表示
    static const char *DiffLabel_(Difficulty d);
};
