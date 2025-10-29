#pragma once
#define NOMINMAX

#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "SceneManager.h"
#include "Camera.h"
#include "Model.h"

#include <DirectXMath.h>
#include <string>

// ClearScene.h 側のクラス宣言にこれを足す
class ClearScene : public IScene {
public:
    ClearScene(float clearTimeSec, int nextStageId, int playedStageId)
        : clearTimeSec_(clearTimeSec)
        , nextStageId_(nextStageId)
        , playedStageId_(playedStageId) {
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

    int menuIndex_ = 0; // 0=Next,1=Select
};
