#pragma once
#include <memory>
#include "IScene.h"

/// <summary>
/// シーンの生成を簡易化するヘルパー関数。<br/>
/// IScene を継承したクラスを生成し、std::unique_ptr<IScene> として返す。
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
/// シーン遷移を管理する最小限のマネージャ。<br/>
/// - ChangeScene() で次のシーンを予約<br/>
/// - Update() の先頭で安全に切替（Finalize/Initialize を自動実行）
/// </summary>
class SceneManager {
public:
    /// <summary>
    /// マネージャの初期化。以後 EngineContext を保持する。
    /// </summary>
    /// <param name="engine">エンジンの共有コンテキスト。</param>
    void Initialize(const EngineContext &engine);

    /// <summary>
    /// フレーム更新処理。<br/>
    /// 必要ならこの時点でシーンを切替する。
    /// </summary>
    /// <param name="dt">経過時間（秒）。</param>
    void Update(float dt);

    /// <summary>
    /// 描画処理。
    /// </summary>
    /// <param name="rc">描画コンテキスト。</param>
    void Draw(const RenderContext &rc);

    /// <summary>
    /// 破棄処理。<br/>
    /// 現在のシーンを Finalize する。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 次のシーンへ切替を予約する。<br/>
    /// （即時ではなく、次フレームの Update 冒頭で切替される）
    /// </summary>
    /// <param name="next">次のシーン。</param>
    void ChangeScene(std::unique_ptr<IScene> next);

    /// <summary>
    /// シーンが存在しているかどうかを返す。
    /// </summary>
    bool HasScene() const { return static_cast<bool>(current_); }

    /// <summary>
    /// 現在のシーンを取得する。<br/>
    /// 必要に応じてダウンキャストして利用する。
    /// </summary>
    IScene *GetCurrent() const { return current_.get(); }

    /// <summary>
    /// テンプレート版のシーン切替。<br/>
    /// 型指定でその場で生成し、次のシーンとして予約する。
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
    /// 次のシーンが予約されていた場合、切替処理を実行する。
    /// </summary>
    void ProcessPendingChange();

private:
    const EngineContext *engine_ = nullptr; // エンジンの共有コンテキスト
    std::unique_ptr<IScene> current_;       // 現在のシーン
    std::unique_ptr<IScene> pending_;       // 次に切り替える予定のシーン
    bool currentInitialized_ = false;       // 現在のシーンが初期化済みかどうか
};
