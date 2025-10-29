#pragma once
#define NOMINMAX

#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "SceneManager.h"
#include "Camera.h"
#include "Model.h"
#include "Transform.h"
#include "Difficulty.h"

#include <string>
#include <array>

class ClearScene : public IScene {
public:
    ClearScene(float clearTimeSec,
        int nextStageId,
        int playedStageId,
        Difficulty diff)
        : clearTimeSec_(clearTimeSec)
        , nextStageId_(nextStageId)
        , playedStageId_(playedStageId)
        , difficulty_(diff) {
    }

    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

private:
    // ===== 参照とシーン管理 =====
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;
    SceneManager *sceneManager_ = nullptr;

    Camera camera_;

    // ====== UI用モデル ======
    Model titlePlateModel_;    // CLEAR TIME 的な板 (normal.obj 代用)
    Model nextStageModel_;     // NEXT STAGE
    Model stageSelectModel_;   // STAGE SELECT

    std::array<Model, 10> digitModel_; // '0'..'9'
    Model spaceModel_;                 // ":" "." " " 代用

    // ====== 背景用モデル (TitleSceneと同じラインナップ) ======
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

    // ====== クリア情報 ======
    float clearTimeSec_ = 0.0f;
    int   nextStageId_ = 0;
    int   playedStageId_ = 1;
    Difficulty difficulty_ = Difficulty::Normal;

    int menuIndex_ = 0; // 0=NEXT STAGE, 1=STAGE SELECT

    // ====== 背景レイアウト用パラメータ ======
    // TitleSceneと同じような「工事現場夜景」を再現するための仮想ワールドサイズ
    float worldW_ = 32.0f;
    float worldH_ = 18.0f;

    // ===== 内部関数 =====
    std::string BuildTimeString_() const;

    void DrawTimeString3D_(
        ID3D12GraphicsCommandList *cmd,
        const std::string &timeText,
        float baseX, float baseY, float z,
        float charW, float charH,
        float spacing
    );

    // 汎用描画ヘルパ（TitleSceneと同じノリ）
    void DrawModel_(
        Model &m,
        const DirectX::XMFLOAT3 &pos,
        const DirectX::XMFLOAT3 &fullScale,
        const DirectX::XMFLOAT3 &rotDeg,
        float alphaMul = 1.0f
    );

    // 背景を描画（TitleScene::DrawTitleBackground_ のコピペ調整版）
    void DrawBackground_();
};
