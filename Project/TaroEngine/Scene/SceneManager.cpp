#include "SceneManager.h"
#include "DirectXCommon.h"

void SceneManager::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;
}

void SceneManager::Update(float deltaTime) {
    // 1) もし前のシーンがまだ残ってるなら、
    //    GPUが使い終わるのを待ってから安全に破棄する
    if (pendingDestroy_) {
        // GPUが今まで積んだドローコマンドを全部完了させる
        engineContext_->directXCommon->WaitForGpu();

        pendingDestroy_->Finalize();
        pendingDestroy_.reset();
    }

    // 2) シーン切り替えリクエストが来てたら、ここで実行する
    if (sceneSwitchRequested_ && pending_) {
        // いまのシーンは、いきなり Finalize/reset しないで退避
        if (currentScene_) {
            pendingDestroy_ = std::move(currentScene_);
        }

        // 新しいシーンを currentScene_ に昇格
        currentScene_ = std::move(pending_);
        sceneSwitchRequested_ = false;

        if (currentScene_) {
            currentScene_->Initialize(engineContext_, renderContext_);
        }
    }

    // 3) 今アクティブなシーンを通常更新
    if (currentScene_) {
        currentScene_->Update(deltaTime);
    }
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}

void SceneManager::Finalize() {
    // GPU待ってから順番に破棄
    if (currentScene_) {
        engineContext_->directXCommon->WaitForGpu();
        currentScene_->Finalize();
        currentScene_.reset();
    }

    if (pendingDestroy_) {
        engineContext_->directXCommon->WaitForGpu();
        pendingDestroy_->Finalize();
        pendingDestroy_.reset();
    }

    pending_.reset();
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene) {
    // ここではまだ切り替えない。ただ「次に切り替えたい」ことだけ覚える
    pending_ = std::move(nextScene);
    sceneSwitchRequested_ = true;
}
