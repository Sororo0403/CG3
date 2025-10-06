#include "GameScene.h"
#include "imgui.h"
#include "SpriteCommon.h"

void GameScene::Initialize(const EngineContext &engine) {
	// スプライト初期化
	sprite_.Initialize(engine.device);

	// 既定のレンダーターゲット解像度
	const uint32_t defaultW = 1280;
	const uint32_t defaultH = 720;

	// ピクセル→NDC変換のための画面サイズ設定
	sprite_.SetViewportSize(defaultW, defaultH);

	// スプライトの基本プロパティ
	sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	sprite_.SetRect(100.0f, 100.0f, 256.0f, 256.0f);
}

void GameScene::Update(float /*dt*/) {
	sprite_.Update();
}

void GameScene::Draw(const EngineContext &engine, const RenderContext &rc) {
	// ImGui デバッグUI
	if (ImGui::Begin("Sprite")) {
		static float x = 100.0f, y = 100.0f, w = 256.0f, h = 256.0f;
		static float col[4] = {1, 1, 1, 1};

		bool posChanged = ImGui::DragFloat2("Pos (px)", &x, 1.0f);
		bool sizeChanged = ImGui::DragFloat2("Size (px)", &w, 1.0f, 1.0f, 4096.0f);
		bool colorChanged = ImGui::ColorEdit4("Color", col);

		if (posChanged || sizeChanged) {
			sprite_.SetRect(x, y, w, h);
		}
		if (colorChanged) {
			sprite_.SetColor(col[0], col[1], col[2], col[3]);
		}

		ImGui::End();
	}

	// 描画設定とスプライト描画
	engine.spriteCommon->ApplyCommonDrawSettings(rc.commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	sprite_.Draw(rc.commandList);
}

void GameScene::Finalize() {
	// 現状では解放対象なし
}

void GameScene::OnResize(uint32_t w, uint32_t h) {
	sprite_.SetViewportSize(w, h);
}