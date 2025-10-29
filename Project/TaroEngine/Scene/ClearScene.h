#pragma once
#define NOMINMAX

#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "SceneManager.h"
#include "Camera.h"
#include "Model.h"
#include "Difficulty.h"

#include <DirectXMath.h>
#include <string>

// ClearScene.h 側のクラス宣言にこれを足す
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
    // 既存のやつ:
    void Initialize(const EngineContext *engine, const RenderContext *render) override;
    void Update(float dt) override;
    void Draw() override;
    void Finalize() override;

private:
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;
	SceneManager *sceneManager_ = nullptr;

    Camera camera_;


    // ★追加
    int   playedStageId_ = 1;

    float clearTimeSec_ = 0.0f;
    int nextStageId_ = 0;


    // ★追加: クリア時の難易度を保持
    Difficulty difficulty_ = Difficulty::Normal;

    int menuIndex_ = 0; // 0=Next,1=Select
};
