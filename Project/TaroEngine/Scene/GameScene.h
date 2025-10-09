#pragma once

#include <cstdint>
#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "Sprite.h"

class GameScene : public IScene {
public:
	/// <summary>
	/// デフォルトコンストラクタ
	/// </summary>
	GameScene() = default;

	/// <summary>
	/// 仮想デストラクタ
	/// </summary>
	~GameScene() override = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="engineContext">エンジンの共有コンテキスト</param>
	void Initialize(const EngineContext *engineContext) override;

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(float deltaTime) override;

	/// <summary>
	/// 描画処理
	/// </summary>
	/// <param name="engineContext">エンジンの共有コンテキスト</param>
	/// <param name="renderContext">描画コンテキスト</param>
	void Draw(const EngineContext *engineContext, const RenderContext *renderContext) override;

	/// <summary>
	/// 解放処理
	/// </summary>
	void Finalize() override;

private:
	Sprite sprite_;					///< スプライト描画オブジェクト
	bool spriteTexLoaded_ = false;	///< テクスチャロード済みフラグ
};
