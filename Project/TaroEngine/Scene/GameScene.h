#pragma once
#include "EngineContext.h"
#include "RenderContext.h"
#include "ModelRenderer.h"
#include "Model.h"
#include "IScene.h"
#include "Camera.h"

/// <summary>
/// OBJ (player.obj) を 1 体だけ描画する最小シーン
/// </summary>
class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Update(float /*deltaTime*/) override {}
    void Draw() override;
    void Finalize() override;

private:
    // 参照
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;

    // 描画リソース
    ModelRenderer renderer_;
    Model         player_;
    Camera        camera_;
};
