#pragma once
#include <memory>
#include "IScene.h"

/// <summary>
/// IScene を継承したクラスを生成し、unique_ptr<IScene> として返すヘルパー関数。
/// </summary>
/// <typeparam name="T">IScene を継承した型。</typeparam>
/// <typeparam name="Args">コンストラクタ引数の型。</typeparam>
/// <param name="args">コンストラクタ引数。</param>
/// <returns>生成されたシーンの unique_ptr。</returns>
template <class T, class... Args>
std::unique_ptr<IScene> MakeScene(Args&&... args) {
	static_assert(std::is_base_of<IScene, T>::value, "T must derive from IScene");
	return std::make_unique<T>(std::forward<Args>(args)...);
}

/// <summary>
/// シーンの生成・切替・破棄を管理する基本的なシーンマネージャ。
/// </summary>
class SceneManager {
public:
	/// <summary>
	/// エンジンの共有コンテキストを保持して初期化する。
	/// </summary>
	/// <param name="engine">エンジンの共有コンテキスト。</param>
	void Initialize(const EngineContext &engine);

	/// <summary>
	/// シーンを更新し、必要に応じて切替処理を行う。
	/// </summary>
	/// <param name="dt">経過時間（秒）。</param>
	void Update(float dt);

	/// <summary>
	/// 現在のシーンを描画する。
	/// </summary>
	/// <param name="rc">描画コンテキスト。</param>
	void Draw(const RenderContext &rc);

	/// <summary>
	/// 現在のシーンを終了処理する。
	/// </summary>
	void Finalize();

	/// <summary>
	/// 次のシーンへの切替を予約する。
	/// </summary>
	/// <param name="next">次のシーン。</param>
	void ChangeScene(std::unique_ptr<IScene> next);

	/// <summary>
	/// シーンが存在するかを判定する。
	/// </summary>
	bool HasScene() const { return static_cast<bool>(current_); }

	/// <summary>
	/// 現在のシーンを取得する。
	/// </summary>
	IScene *GetCurrent() const { return current_.get(); }

	/// <summary>
	/// テンプレート版のシーン切替。型を指定してシーンを生成・予約する。
	/// </summary>
	/// <typeparam name="T">IScene を継承した型。</typeparam>
	/// <typeparam name="Args">コンストラクタ引数の型。</typeparam>
	/// <param name="args">コンストラクタ引数。</param>
	template <class T, class... Args>
	void ChangeSceneT(Args&&... args) {
		ChangeScene(MakeScene<T>(std::forward<Args>(args)...));
	}

private:
	/// <summary>
	/// 次のシーンが予約されている場合に切替を実行する。
	/// </summary>
	void ProcessPendingChange();

private:
	const EngineContext *engine_ = nullptr;   // エンジンの共有コンテキスト
	std::unique_ptr<IScene> current_;         // 現在のシーン
	std::unique_ptr<IScene> pending_;         // 次に切り替える予定のシーン
	bool currentInitialized_ = false;         // 現在のシーンが初期化済みかどうか
};
