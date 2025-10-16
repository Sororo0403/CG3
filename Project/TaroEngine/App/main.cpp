#include "DirectXCommon.h"
#include "WinApp.h"
#include "EngineContext.h"
#include "RenderContext.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "FileLogger.h"
#include "OutputLogger.h"
#include "LoggerManager.h"

#include <memory>
#include <chrono>

/// <summary>
/// アプリのエントリポイント。
/// </summary>
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// ===============================
	// アプリ初期化
	// ===============================
	// winApp
	std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	// DirectX
	std::unique_ptr<DirectXCommon> directXCommon = std::make_unique<DirectXCommon>();
	directXCommon->Initialize(winApp.get());

	// リサイズ時の再生成
	winApp->SetOnResize([rawDx = directXCommon.get()](uint32_t w, uint32_t h, UINT state) {
		if (state == SIZE_MINIMIZED) { return; }
		rawDx->Resize(w, h);
		});

	// EngineContext
	EngineContext engineContext{};
	engineContext.directXCommon = directXCommon.get();

	RenderContext renderContext{};

	// ===============================
	// シーンマネージャ初期化
	// ===============================
	SceneManager sceneManager;
	sceneManager.Initialize(&engineContext, &renderContext);
	sceneManager.ChangeScene(std::make_unique<GameScene>());

	// ===============================
	// ロガー設定
	// ===============================
	LoggerManager loggerManager;
	loggerManager.AddLogger(std::make_shared<OutputLogger>());

	std::shared_ptr<FileLogger> fileLogger = std::make_shared<FileLogger>();
	fileLogger->SetFilePath("log.txt");
	loggerManager.AddLogger(fileLogger);

	engineContext.loggerManager = &loggerManager;

	// ===============================
	// メインループ
	// ===============================
	using Clock = std::chrono::high_resolution_clock;

	constexpr float kDeltaTimeClampMin = 1.0f / 240.0f;
	constexpr float kDeltaTimeClampMax = 1.0f / 15.0f;
	constexpr float kClearColor[4] = {0.1f, 0.25f, 0.5f, 1.0f};

	auto prev = Clock::now();
	bool running = true;

	while (running) {
		// Windowsメッセージ処理
		if (winApp->ProcessMessage()) {
			break;
		}

		// 経過時間(秒)
		const auto now = Clock::now();
		float deltaTime = std::chrono::duration<float>(now - prev).count();
		prev = now;

		// dt のクランプ（下限・上限）
		if (deltaTime < kDeltaTimeClampMin) { deltaTime = kDeltaTimeClampMin; }
		if (deltaTime > kDeltaTimeClampMax) { deltaTime = kDeltaTimeClampMax; }

		// --- 更新 ---
		sceneManager.Update(deltaTime);

		// --- 描画 ---
		directXCommon->PreDraw(kClearColor);

		renderContext.commandList = directXCommon->GetCommandList();
		sceneManager.Draw();

		directXCommon->PostDraw();
	}

	// ===============================
	// 終了処理
	// ===============================
	sceneManager.Finalize();
	directXCommon->Finalize();
	winApp->Finalize();

	return 0;
}
