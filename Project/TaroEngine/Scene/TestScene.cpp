#define NOMINMAX
#include "TestScene.h"
#include "ModelRenderer.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "DirectXCommon.h"
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

void TestScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
	// コンテキスト保存
	engineContext_ = engineContext;
	renderContext_ = renderContext;

	// モデル初期化
	testModel_.Initialize(engineContext_->directXCommon->GetDevice(),
		"Resources/Models/teapot.obj");

	testModelTransform_.pos = {0.0f, 0.0f, 0.0f};
	testModelTransform_.rot = {0.0f, 0.0f, 0.0f};
	testModelTransform_.scale = {1.0f, 1.0f, 1.0f};

	// カメラ初期化
	auto *dx = engineContext_->directXCommon;
	float w = static_cast<float>(dx->GetWidth());
	float h = std::max(1.0f, static_cast<float>(dx->GetHeight()));
	float aspect = w / h;

	camera_.Initialize(
		{0.0f, 0.5f, -6.0f},
		{0.0f, 0.0f,  0.0f},
		60.0f,
		aspect,
		0.1f,
		1000.0f
	);
	camera_.LookAt({0.0f, 0.5f, -6.0f}, {0.0f, 0.0f, 0.0f});
	camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

void TestScene::Update(float deltaTime) {
	// 画面サイズが変わってもカメラのアスペクトを追従
	auto *dx = engineContext_->directXCommon;
	camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

	// テストモデルを回す
	testModelTransform_.rot.y += deltaTime * 0.7f;
}

void TestScene::Draw() {
	auto *renderer = renderContext_->modelRenderer;
	auto *cmd = renderContext_->commandList;
	auto *dx = engineContext_->directXCommon;
	if (!renderer || !cmd || !dx) return;

	// モデル描画
	renderer->Begin(cmd, dx, camera_);
	renderer->Draw(cmd, testModel_, testModelTransform_);
	renderer->End(cmd);
}

void TestScene::Finalize() {}
