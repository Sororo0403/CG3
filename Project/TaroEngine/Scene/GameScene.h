#pragma once
#include "IScene.h"
#include "Sprite.h"
#include "SpriteCommon.h"

/// <summary>
/// 実ゲーム用の単独シーン。
/// </summary>
class GameScene : public IScene {
public:
    /// <summary>
    /// 共有リソースを受け取って初期化（DI）
    /// </summary>
    void Initialize(const EngineContext &engine) override;

    /// <summary>
    /// ゲーム更新
    /// </summary>
    void Update(float dt) override;

    /// <summary>
    /// 描画：共通PSO適用→個別スプライト描画<
    /// /summary>
    void Draw(const EngineContext &engine, const RenderContext &rc) override;

    /// <summary>
    /// 終了処理
    /// </summary>
    void Finalize() override;

private:
    Sprite sprite_;
};
