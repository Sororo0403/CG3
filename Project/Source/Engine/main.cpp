#include "DirectX/DirectXCommon.h"
#include "WinApp/WinApp.h"
#include "Input/Input.h"
#include "Sprite/SpriteRenderer.h"
#include "Shader/ShaderCompiler.h"
#include <Windows.h>
#include <string>

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// winApp
	const LONG kClientWidth = 1280;
	const LONG kClientHeight = 720;
	const std::wstring kWindowTitle = L"GE3";

	std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
	winApp->Initialize(kClientWidth, kClientHeight, kWindowTitle);

	// DirectX
	std::unique_ptr<DirectXCommon> directXCommon = std::make_unique<DirectXCommon>();
	directXCommon->Initialize(winApp.get());

	// Input
	std::unique_ptr<Input> input = std::make_unique<Input>();
	input->Initialize(winApp->GetHInstance(), winApp->GetHwnd());

	// ShaderCompiler
	ShaderCompiler shaderCompiler;

	// TextureManager
	TextureManager textureManager;
	textureManager.Initialize(directXCommon.get());

	// SpriteRenderer
	SpriteRenderer spriteRenderer;
	spriteRenderer.Initialize(directXCommon.get(), &shaderCompiler);

	// クリアカラー
	const float kClearColor[4] = {0.0f,1.0f,1.0f,1.0f};

	// テクスチャ
	uint32_t textureId = textureManager.LoadTexture("Resources/uvChecker.png");
	const float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	while (true) {
		// Windowsメッセージ処理
		if (winApp->ProcessMessage()) {
			break;
		}

		// Input更新
		input->Update();

		// 描画前処理
		directXCommon->PreDraw(kClearColor);

		// Sprite描画
		spriteRenderer.DrawSprite(
			textureId,
			0.0f, 0.0f,
			256.0f, 256.0f,
			0.0f, 0.0f,
			1.0f, 1.0f,
			color
		);

		// 描画終了処理
		directXCommon->PostDraw();
	}

	return 0;
}
