#pragma once
#include "IScene.h"
#include "Sprite.h"

/// <summary>
/// スプライトを管理・描画するメインのゲームシーン。
/// </summary>
class GameScene : public IScene {
public:
    /// <summary>
    /// シーンを初期化し、スプライトを構築する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    void Initialize(const EngineContext &engine) override;

    /// <summary>
    /// シーンを更新し、スプライトの状態を反映する。
    /// </summary>
    /// <param name="dt">経過時間（秒）。</param>
    void Update(float dt) override;

    /// <summary>
    /// シーンを描画し、スプライトを表示する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    /// <param name="rc">描画コンテキスト。</param>
    void Draw(const EngineContext &engine, const RenderContext &rc) override;

    /// <summary>
    /// シーン固有のリソースを解放する。
    /// </summary>
    void Finalize() override;

    /// <summary>
    /// ウィンドウサイズ変更に応じてスプライトを調整する。
    /// </summary>
    /// <param name="w">ウィンドウの幅。</param>
    /// <param name="h">ウィンドウの高さ。</param>
    void OnResize(uint32_t w, uint32_t h);

private:
	Sprite sprite_; // 画面に張り付くスプライト
};
