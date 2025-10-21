#pragma once

#include "EngineContext.h"
#include "RenderContext.h"
#include "Model.h"
#include "IScene.h"
#include "Camera.h"
#include "Player.h"
#include "SolidBlock.h"

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float deltaTime) override;
    void Draw() override;
    void Finalize() override;

private:
    // 参照
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;

    // 描画リソース
    Camera camera_;

	// ゲームオブジェクト
	Player player_;
	SolidBlock solidBlock_;
};
