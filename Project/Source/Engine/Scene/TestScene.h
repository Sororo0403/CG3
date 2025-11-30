#pragma once

#include "IScene.h"
#include "Model.h"
#include "Camera.h"

class TestScene : public IScene {
public:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="engineContext">エンジンの共有コンテキスト</param>
	/// <param name="renderContext">エンジンの描画コンテキスト</param>
	void Initialize(const EngineContext *engineContext, const RenderContext *renderContext) override;

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(float deltaTime) override;

	/// <summary>
	/// 描画処理
	/// </summary>
	void Draw() override;

	/// <summary>
	/// 終了処理
	/// </summary>
	void Finalize() override;

private:
	// テスト用モデル
	Model testModel_;
	Transform testModelTransform_;

	// カメラ
	Camera camera_;
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
};
