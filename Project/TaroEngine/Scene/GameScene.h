#pragma once
#include <optional>
#include <cstdint>
#include "EngineContext.h"
#include "RenderContext.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "IScene.h"

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext) override;
    void Update(float deltaTime) override;
    void Draw(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Finalize() override;

private:
    // UI 状態
    float uiX_ = 100.0f, uiY_ = 100.0f;
    float uiW_ = 256.0f, uiH_ = 256.0f;
    float uiCol_[4] = {1, 1, 1, 1};

    Sprite sprite_{};
    TextureManager texMgr_{};
    std::optional<TextureHandle> spriteTex_; // ← SetTexture ではなく TextureHandle を保持
};
