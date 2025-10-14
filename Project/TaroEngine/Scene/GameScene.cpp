// GameScene.cpp
#include "GameScene.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "EngineContext.h"
#include "LoggerManager.h"
#include "LogLevel.h"

#include "imgui.h"
#include <DirectXMath.h>
#include <format>
using namespace DirectX;

// ImGui が SRV[0] を使用している想定 → スプライトは [1] から
namespace { constexpr UINT kSpriteSrvStartIndex = 1; }

// 行列ユーティリティ（Transpose 済みの float[16] を作る簡易版）
static void MakeViewProj_T(float *out16, float eyeX, float eyeY, float eyeZ,
	float tgtX, float tgtY, float tgtZ,
	float upX, float upY, float upZ,
	float fovY, float aspect, float zn, float zf) {
	XMVECTOR eye = XMVectorSet(eyeX, eyeY, eyeZ, 1.0f);
	XMVECTOR tgt = XMVectorSet(tgtX, tgtY, tgtZ, 1.0f);
	XMVECTOR up = XMVectorSet(upX, upY, upZ, 0.0f);
	XMMATRIX V = XMMatrixLookAtRH(eye, tgt, up);
	XMMATRIX P = XMMatrixPerspectiveFovRH(fovY, aspect, zn, zf);
	XMMATRIX VP = XMMatrixTranspose(V * P);
	XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4 *>(out16), VP);
}

void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
	engineContext_ = engineContext;
	renderContext_ = renderContext;

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::INFO, "GameScene: 初期化開始");
	}

	// === Sprite ===
	sprite_.Initialize(engineContext_->device);
	sprite_.SetViewportSize(1280, 720);
	sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
	sprite_.SetRect(uiX_, uiY_, uiW_, uiH_);

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::DEBUG, std::format(
			"Sprite 初期化完了: Rect=({}, {}, {}, {})", uiX_, uiY_, uiW_, uiH_));
	}

	// === SRV ヒープと紐付け ===
	texMgr_.Initialize(
		engineContext_->device,
		engineContext_->directXCommon->GetSrvHeap(),
		kSpriteSrvStartIndex);

	spriteTex_.reset();

	// === 3D共通部 & plane ===
	obj3dCommon_.Initialize(engineContext_->directXCommon);
	plane_.Initialize(&obj3dCommon_);
	plane_.LoadObj(L"Resources/plane.obj");
	plane_.SetScale(pScl_[0], pScl_[1], pScl_[2]);
	plane_.SetColor(pCol_[0], pCol_[1], pCol_[2], pCol_[3]);
	planeTex_.reset();

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::INFO, "Plane モデルを読み込みました (Resources/plane.obj)");
	}

	// === カメラ行列 ===
	MakeViewProj_T(viewProj_, camPos_[0], camPos_[1], camPos_[2],
		0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::DEBUG, "カメラ行列を設定しました");
	}

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::INFO, "GameScene: 初期化完了");
	}
}

void GameScene::Update(float /*deltaTime*/) {
	// === Plane の状態更新 ===
	plane_.SetPosition(pPos_[0], pPos_[1], pPos_[2]);
	plane_.SetRotation(pRot_[0], pRot_[1], pRot_[2]);
	plane_.SetScale(pScl_[0], pScl_[1], pScl_[2]);
	plane_.SetColor(pCol_[0], pCol_[1], pCol_[2], pCol_[3]);

	// === CB更新 ===
	plane_.Update(viewProj_, camPos_);
	sprite_.Update();
}

void GameScene::Draw() {
	// === 初回ロード ===
	if (!spriteTex_.has_value()) {
		spriteTex_ = texMgr_.Load(renderContext_->commandList, L"Resources/uvChecker.png");
		sprite_.SetTextureView(spriteTex_->view);

		if (engineContext_->loggerManager) {
			engineContext_->loggerManager->Log(LogLevel::INFO, "Sprite テクスチャをロードしました (uvChecker.png)");
		}
	}

	if (!planeTex_.has_value()) {
		planeTex_ = texMgr_.Load(renderContext_->commandList, L"Resources/uvChecker.png");
		plane_.SetTextureSrv(planeTex_->view.gpu);

		if (engineContext_->loggerManager) {
			engineContext_->loggerManager->Log(LogLevel::INFO, "Plane テクスチャをロードしました (uvChecker.png)");
		}
	}

	// === ImGui ===
	if (ImGui::Begin("Sprite")) {
		bool moved = ImGui::DragFloat2("Pos (px)", &uiX_, 1.0f);
		bool sized = ImGui::DragFloat2("Size (px)", &uiW_, 1.0f, 1.0f, 4096.0f);
		bool recol = ImGui::ColorEdit4("Color", uiCol_);
		if (moved || sized) { sprite_.SetRect(uiX_, uiY_, uiW_, uiH_); }
		if (recol) { sprite_.SetColor(uiCol_[0], uiCol_[1], uiCol_[2], uiCol_[3]); }
		ImGui::End();
	}
	if (ImGui::Begin("Plane")) {
		ImGui::DragFloat3("Pos", pPos_, 0.05f);
		ImGui::DragFloat3("Rot(rad)", pRot_, 0.01f);
		ImGui::DragFloat3("Scale", pScl_, 0.05f, 0.01f, 100.0f);
		ImGui::ColorEdit4("Color", pCol_);
		ImGui::End();
	}

	// === 描画 ===
	plane_.Draw(renderContext_->commandList);
	engineContext_->spriteCommon->ApplyCommonDrawSettings(
		renderContext_->commandList,
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	sprite_.Draw(renderContext_->commandList);
}

void GameScene::Finalize() {
	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::INFO, "GameScene: 終了処理開始");
	}

	spriteTex_.reset();
	planeTex_.reset();

	if (engineContext_->loggerManager) {
		engineContext_->loggerManager->Log(LogLevel::DEBUG, "GameScene: リソースを解放しました");
		engineContext_->loggerManager->Log(LogLevel::INFO, "GameScene: 終了処理完了");
	}
}
