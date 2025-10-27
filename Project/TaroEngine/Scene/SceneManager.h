#pragma once
#include <memory>
#include "IScene.h"
#include "EngineContext.h"
#include "RenderContext.h"

/// <summary>
/// シーン管理:
/// - currentScene_ が今表示してるシーン
/// - ChangeScene() が呼ばれると pending_ に次シーンを入れる
/// - Update() 冒頭で安全に切替:
///    古いシーンは pendingDestroy_ に避難して、GPU待ちしてから破棄
/// </summary>
class SceneManager {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext);
    void Update(float deltaTime);
    void Draw();
    void Finalize();

    /// <summary>
    /// 次のシーンへの切替をリクエストする（即Finalizeしない）
    /// </summary>
    void ChangeScene(std::unique_ptr<IScene> nextScene);

private:
    const EngineContext *engineContext_ = nullptr;
    const RenderContext *renderContext_ = nullptr;

    // 現在アクティブなシーン
    std::unique_ptr<IScene> currentScene_;

    // 次に切り替えたいシーン（まだ表示してない）
    std::unique_ptr<IScene> pending_;

    // 直前まで使ってたシーン。
    // GPUが前フレームまでの描画をまだ参照しているかもしれないので、
    // WaitForGpu() するまで保持しておく。
    std::unique_ptr<IScene> pendingDestroy_;

    // ChangeScene() 呼ばれたら true になる
    bool sceneSwitchRequested_ = false;
};
