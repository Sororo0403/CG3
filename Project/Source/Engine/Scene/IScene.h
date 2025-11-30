#pragma once

#include "EngineContext.h"
#include "RenderContext.h"

class IScene {
public:
	/// <summary>
	/// 仮想デストラクタ
	/// </summary>
	virtual ~IScene() = default;

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="engineContext">エンジンの共有コンテキスト</param>
	/// <param name="engineContext">描画コンテキスト</param>
	virtual void Initialize(const EngineContext *engineContext,const RenderContext *renderContext) = 0;

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	virtual void Update(float deltaTime) = 0;

	/// <summary>
	/// 描画処理
	/// </summary>
	virtual void Draw() = 0;

	/// <summary>
	/// 解放処理
	/// </summary>
	virtual void Finalize() = 0;

protected:
	const EngineContext *engineContext_ = nullptr; // エンジンの共有コンテキスト
	const RenderContext *renderContext_ = nullptr; // 描画コンテキスト
};
