#include "GameScene.h"

void GameScene::Initialize(const EngineContext &engine) {
    sprite_.Initialize(engine.device);
}

void GameScene::Update(float /*dt*/) {
    sprite_.Update();
}

void GameScene::Draw(const EngineContext &engine, const RenderContext &rc) {
    engine.spriteCommon->ApplyCommonDrawSettings(
        rc.cmdList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    sprite_.Draw(rc.cmdList);
}

void GameScene::Finalize() {
    // 特に解放処理なし（必要なら追加）
}
