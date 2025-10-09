#pragma once
#include "IScene.h"
#include "Sprite.h"
#include <memory>

struct EngineContext;
struct RenderContext;
#include "TextureManager.h"
#include "Texture2D.h"

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext) override;
    void Update(float deltaTime) override;
    void Draw(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Finalize() override;

private:
    Sprite sprite_;

    // テクスチャ分離後のメンバ
    TextureManager texMgr_;                // SRV ヒープ管理＋キャッシュ
    std::shared_ptr<Texture2D>      spriteTex_;             // 表示用テクスチャ（共有）

    // ImGui 調整用
    float uiX_ = 100.0f, uiY_ = 100.0f, uiW_ = 256.0f, uiH_ = 256.0f;
    float uiCol_[4] = {1,1,1,1};
};