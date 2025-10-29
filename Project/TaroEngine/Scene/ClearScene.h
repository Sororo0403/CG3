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

    // UI用モデル
    Model titlePlateModel_;    // CLEAR TIME 的な板 (normal.obj を仮使用)
    Model nextStageModel_;     // NEXT STAGE
    Model stageSelectModel_;   // STAGE SELECT

    std::array<Model, 10> digitModel_; // 0..9
    Model spaceModel_;               // スペース (space.obj) これ ":" "." のダミーにも使う

    // クリア情報
    float clearTimeSec_ = 0.0f;
    int   nextStageId_ = 0;
    int   playedStageId_ = 1;
    Difficulty difficulty_ = Difficulty::Normal;

    int menuIndex_ = 0; // 0=NextStage,1=StageSelect

    std::string BuildTimeString_() const;

    void DrawTimeString3D_(
        ID3D12GraphicsCommandList *cmd,
        const std::string &timeText,
        float baseX, float baseY, float z,
        float charW, float charH,
        float spacing
    );
};
