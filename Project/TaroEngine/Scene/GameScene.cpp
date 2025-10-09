#include "GameScene.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "imgui.h"

// ImGui が SRV[0] を使用している想定 → スプライトは [1] に置く
namespace {
	constexpr UINT kSpriteSrvIndex = 1;
}

void GameScene::Initialize(const EngineContext *engineContext) {
	// === スプライト初期化 ===
	sprite_.Initialize(engineContext->device);

	// 画面サイズ（ピクセル）
	const uint32_t defaultW = 1280;
	const uint32_t defaultH = 720;
	sprite_.SetViewportSize(defaultW, defaultH);

	// 色と矩形設定
	sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	sprite_.SetRect(100.0f, 100.0f, 256.0f, 256.0f);

	// テクスチャは Draw() の初回でロードする
	spriteTexLoaded_ = false;
}

void GameScene::Update(float /*deltaTime*/) {
	// 定数バッファ更新など
	sprite_.Update();
}

void GameScene::Draw(const EngineContext *engineContext, const RenderContext *renderContext) {
	// === 初回のみテクスチャをロード ===
	if (!spriteTexLoaded_) {
		sprite_.LoadTextureFromFile(
			engineContext->device,
			renderContext->commandList,
			L"Resources/uvChecker.png",        // ← テクスチャファイル
			engineContext->directXCommon->GetSrvHeap(),     // SRVヒープ（共用）
			kSpriteSrvIndex                    // SRVインデックス
		);
		spriteTexLoaded_ = true;
	}

	// === ImGui デバッグUI ===
	if (ImGui::Begin("Sprite")) {
		static float x = 100.0f, y = 100.0f, w = 256.0f, h = 256.0f;
		static float col[4] = {1, 1, 1, 1};

		bool moved = ImGui::DragFloat2("Pos (px)", &x, 1.0f);
		bool sized = ImGui::DragFloat2("Size (px)", &w, 1.0f, 1.0f, 4096.0f);
		bool recol = ImGui::ColorEdit4("Color", col);

		if (moved || sized) { sprite_.SetRect(x, y, w, h); }
		if (recol) { sprite_.SetColor(col[0], col[1], col[2], col[3]); }

		ImGui::End();
	}

	// === スプライト描画 ===
	// 共通PSOとルートシグネチャ適用
	engineContext->spriteCommon->ApplyCommonDrawSettings(
		renderContext->commandList,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 描画
	sprite_.Draw(renderContext->commandList);
}

void GameScene::Finalize() {
	// 今のところ特に解放対象なし
}
