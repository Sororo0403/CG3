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
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;
    SceneManager *sceneManager_ = nullptr;

    Camera camera_;

    // UI用モデル（看板や文字）
    Model titlePlateModel_;    // "CLEAR TIME" 的な板 (normal.obj などを流用)
    Model nextStageModel_;     // "NEXT STAGE"
    Model stageSelectModel_;   // "STAGE SELECT"

    std::array<Model, 10> digitModel_; // 0..9
    Model spaceModel_;                 // ":" "." " " など記号用にまとめて使う

    // クリア情報
    float clearTimeSec_ = 0.0f;
    int   nextStageId_ = 0;
    int   playedStageId_ = 1;
    Difficulty difficulty_ = Difficulty::Normal;

    // メニューどっち選んでるか
    // 0 = NEXT STAGE, 1 = STAGE SELECT
    int menuIndex_ = 0;

    // "mm:ss.mmm" 文字列を組む
    std::string BuildTimeString_() const;

    // 1文字ずつ並べて描画（数字/コロン/ドット）
    void DrawTimeString3D_(
        ID3D12GraphicsCommandList *cmd,
        const std::string &timeText,
        float baseX, float baseY, float z,
        float charW, float charH,
        float spacing
    );
};
