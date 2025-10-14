#pragma once
#include <memory>
#include "IScene.h"

class SceneManager {
public:
	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="engineContext">エンジンの共有コンテキスト</param>
	/// <param name="renderContext">描画コンテキスト</param>
	void Initialize(const EngineContext *engineContext, const RenderContext *renderContext);

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間(秒)</param>
	void Update(float deltaTime);

	/// <summary>
	/// 描画処理
	/// </summary>
	void Draw();

	/// <summary>
	/// 解放処理
	/// </summary>
	void Finalize();

	/// <summary>
	/// 次のシーンへの切替を予約
	/// </summary>
	/// <param name="nextScene">次のシーン</param>
	void ChangeScene(std::unique_ptr<IScene> nextScene);

private:
	/// <summary>
	/// 次のシーンが予約されている場合に切替を実行
	/// </summary>
	void ProcessPendingChange();

private:
	const EngineContext *engineContext_ = nullptr; // エンジンの共有コンテキスト
	const RenderContext *renderContext_ = nullptr; // 描画コンテキスト
	std::unique_ptr<IScene> currentScene_;		   // 現在のシーン
	std::unique_ptr<IScene> pending_;			   // 次に切り替える予定のシーン
};
