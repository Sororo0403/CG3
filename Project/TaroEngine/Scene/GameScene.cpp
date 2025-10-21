#define NOMINMAX
#include "GameScene.h"
#include "DirectXCommon.h"
#include <DirectXMath.h>

using namespace DirectX;

void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;

    auto *dx = engineContext_->directXCommon;

    // モデル
    player_.Initialize(dx->GetDevice(), "Resources/Model/Player/player.obj", engineContext->input);
    solidBlock_.Initialize(dx->GetDevice(), "Resources/Model/Block/solid_block.obj");

    // カメラ設定
    camera_.Reset();
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    camera_.SetEye({0.0f, 1.2f, -10.0f});
    camera_.SetTarget({0.0f, 1.2f,   0.0f});
    camera_.SetUp({0.0f, 1.0f,   0.0f});
    camera_.SetPerspective(120.0f, camera_.GetAspect(), 0.1f, 1000.0f);
}

void GameScene::Update(float deltaTime) {
    player_.Update(deltaTime);
}

void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    // 画面サイズ変化対応
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // ✨ 非転置の V/P をそのまま取得して渡す（Begin 内で転置してCBへ）
    XMMATRIX V = camera_.GetViewMatrix();
    XMMATRIX P = camera_.GetProjMatrix();

    renderContext_->modelRenderer->Begin(cmd, V, P);
    renderContext_->modelRenderer->Draw(cmd, player_.GetModel(), player_.GetTransform());
    renderContext_->modelRenderer->Draw(cmd, solidBlock_.GetModel(), solidBlock_.GetTransform());
    renderContext_->modelRenderer->End(cmd);
}

void GameScene::Finalize() {}
