#pragma once
#include <cstdint>
#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "Sprite.h"

/// <summary>
/// テクスチャ付きスプライトを表示するシーン
/// </summary>
class GameScene : public IScene {  // ★ IScene を public 継承
public:
    GameScene() = default;
    ~GameScene() override = default; // 重要：仮想デストラクタ

    // ===== IScene overrides =====
    void Initialize(const EngineContext &engine) override;
    void Update(float deltaTime) override;
    void Draw(const EngineContext &engine, const RenderContext &rc) override;
    void OnResize(uint32_t w, uint32_t h);
    void Finalize() override;

private:
    Sprite sprite_;
    bool spriteTexLoaded_ = false;
};
