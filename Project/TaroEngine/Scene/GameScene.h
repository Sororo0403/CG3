#pragma once
#include "EngineContext.h"
#include "Sprite.h"
#include "SpriteCommon.h"

/// <summary>
/// 実ゲーム用の単独シーン。シーンマネージャなしで直結運用。
/// </summary>
class GameScene {
public:
    /// <summary>共有リソースを受け取って初期化（DI）</summary>
    void Initialize(const EngineContext &engine) {
        // Sprite はデバイスがあれば初期化できる
        sprite_.Initialize(engine.device);
    }

    /// <summary>ゲーム更新</summary>
    void Update(float /*dt*/) {
        sprite_.Update();
    }

    /// <summary>描画：共通PSO適用→個別スプライト描画</summary>
    void Draw(const EngineContext &engine, const RenderContext &rc) {
        engine.spriteCommon->ApplyCommonDrawSettings(
            rc.cmdList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        sprite_.Draw(rc.cmdList);
    }

    void Finalize() {
        // 特になし
	}

private:
    Sprite sprite_;
};
