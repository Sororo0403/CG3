#include "SceneManager.h"

void SceneManager::Initialize(const EngineContext &engine) {
    engine_ = &engine;
}

void SceneManager::Finalize() {
    if (current_) {
        current_->Finalize();
        current_.reset();
        currentInitialized_ = false;
    }

    pending_.reset();
    engine_ = nullptr;
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> next) {
    // 次フレームの Update 冒頭で確実に切り替える
    pending_ = std::move(next);
}

void SceneManager::ProcessPendingChange() {
    if (!pending_) return;

    // 既存を終了
    if (current_) {
        current_->Finalize();
        current_.reset();
        currentInitialized_ = false;
    }

    // 切替
    current_ = std::move(pending_);
    if (current_ && engine_) {
        current_->Initialize(*engine_);
        currentInitialized_ = true;
    }
}

void SceneManager::Update(float dt) {
    // 切替は Update の先頭で行う（安全に）
    ProcessPendingChange();

    if (currentInitialized_ && current_) {
        current_->Update(dt);
    }
}

void SceneManager::Draw(const RenderContext &rc) {
    if (currentInitialized_ && current_ && engine_) {
        current_->Draw(*engine_, rc);
    }
}
