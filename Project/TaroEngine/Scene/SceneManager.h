#pragma once
#include <memory>
#include "IScene.h"

/// <summary>
/// シーンの生成を簡易化するヘルパー。
/// </summary>
template <class T, class... Args>
std::unique_ptr<IScene> MakeScene(Args&&... args) {
    static_assert(std::is_base_of<IScene, T>::value, "T must derive from IScene");
    return std::make_unique<T>(std::forward<Args>(args)...);
}

/// <summary>
/// シーン遷移を管理する最小限のマネージャ。
/// - ChangeScene() で次のシーンを予約
/// - Update() の先頭で安全に切替（Finalize/Initialize を自動）
/// </summary>
class SceneManager {
public:
    /// <summary>
    /// マネージャの初期化。以後 EngineContext を保持。
    /// </summary>
    void Initialize(const EngineContext &engine);

    /// <summary>
    /// フレーム更新。必要ならこの時点でシーンを切替。
    /// </summary>
    void Update(float dt);

    /// <summary>描画。</summary>
    void Draw(const RenderContext &rc);

    /// <summary>
    /// 破棄。現在シーンを Finalize。
    /// </summary>
    void Finalize();

    /// <summary>
    /// 次のシーンへ切替予約（即時ではなく次フレームの Update 冒頭で切替）。
    /// </summary>
    void ChangeScene(std::unique_ptr<IScene> next);

    /// <summary>
    /// シーンが存在しているか。
    /// </summary>
    bool HasScene() const { return static_cast<bool>(current_); }

    /// <summary>
    /// 現在シーンを取得（必要に応じてダウンキャストして使う）。
    /// </summary>
    IScene *GetCurrent() const { return current_.get(); }

    /// <summary>
    /// テンプレート版：型指定でその場で生成して切替予約。
    /// </summary>
    template <class T, class... Args>
    void ChangeSceneT(Args&&... args) {
        ChangeScene(MakeScene<T>(std::forward<Args>(args)...));
    }

private:
    /// <summary>
    /// 次のシーンが予約されていたら切替処理を行う。
    /// </summary>
    void ProcessPendingChange();

private:
    const EngineContext *engine_ = nullptr;
    std::unique_ptr<IScene> current_;
    std::unique_ptr<IScene> pending_;
    bool currentInitialized_ = false;
};
