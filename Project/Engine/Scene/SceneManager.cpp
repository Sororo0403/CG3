#include "SceneManager.h"
#include "Scene.h"

void SceneManager::ChangeScene(std::unique_ptr<Scene> scene) {
    currentScene_ = std::move(scene);
    currentScene_->Initialize();
}

void SceneManager::Update() {
    if (currentScene_) {
        currentScene_->Update();
    }
}

void SceneManager::Draw() {
    if (currentScene_) {
        currentScene_->Draw();
    }
}
