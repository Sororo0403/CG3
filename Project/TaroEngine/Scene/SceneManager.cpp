#include "SceneManager.h"

void SceneManager::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
	engineContext_ = engineContext;
	renderContext_ = renderContext;
}

void SceneManager::Update(float deltaTime) {
	ProcessPendingChange();

	if (currentScene_) {
		currentScene_->Update(deltaTime);
	}
}

void SceneManager::Draw() {
	if (currentScene_) {
		currentScene_->Draw(engineContext_, renderContext_);
	}
}

void SceneManager::Finalize() {
	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset();
	}

	pending_.reset();
}

void SceneManager::ChangeScene(std::unique_ptr<IScene> nextScene) {
	pending_ = std::move(nextScene);
}

void SceneManager::ProcessPendingChange() {
	if (!pending_) return;

	if (currentScene_) {
		currentScene_->Finalize();
		currentScene_.reset();
	}

	currentScene_ = std::move(pending_);

	if (currentScene_) {
		currentScene_->Initialize(engineContext_);
	}
}
