#include "DirectXCommon.h"
#include "WinApp.h"
#include "SpriteCommon.h"
#include "ShaderCompiler.h"
#include "EngineContext.h"
#include "GameScene.h"
#include "SceneManager.h"
#include "OutputLogger.h"
#include "MultiLogger.h"
#include "FileLogger.h"
#include <memory>
#include <chrono>

/// <summary>
/// アプリのエントリポイント。
/// </summary>
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
	// ===============================
	// 基盤初期化（RAII）
	// ===============================
	std::unique_ptr<WinApp> winApp = std::make_unique<WinApp>();
	winApp->Initialize();

	std::unique_ptr<DirectXCommon> dx = std::make_unique<DirectXCommon>();
	dx->Initialize(winApp.get());

	// リサイズ時の再生成
	winApp->SetOnResize([rawDx = dx.get()](uint32_t w, uint32_t h, UINT state) {
		if (state == SIZE_MINIMIZED) return;
		rawDx->Resize(w, h);
		});

	// ===============================
	// Sprite 共通パイプライン（アプリ全体で1回だけ）
	// ===============================
	ShaderCompiler compiler;
	compiler.Initialize();

	std::unique_ptr<SpriteCommon> spriteCommon = std::make_unique<SpriteCommon>();
	spriteCommon->Initialize(dx->GetDevice());

	SpriteCommon::PipelineFormats formats{};
	formats.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	formats.dsvFormat = DXGI_FORMAT_UNKNOWN;
	formats.numRenderTargets = 1;

	spriteCommon->CreateGraphicsPipeline(
		compiler,
		dx->GetDevice(),
		L"Resources/Shaders/SpriteVS.hlsl",
		L"Resources/Shaders/SpritePS.hlsl",
		formats,
		L"main", L"main");

	// ===============================
	// DI: EngineContext を用意
	// ===============================
	EngineContext engine{};
	engine.directXCommon = dx.get();
	engine.device = dx->GetDevice();
	engine.spriteCommon = spriteCommon.get();
	engine.multiLogger = std::make_unique<MultiLogger>();
	engine.multiLogger->AddLogger(std::make_shared<OutputLogger>());

	std::filesystem::create_directories("Logs");
	auto fileLogger = std::make_shared<FileLogger>("Logs/app.log");
	if (fileLogger->IsOpen()) {
		engine.multiLogger->AddLogger(fileLogger);
		engine.multiLogger->Log(LogLevel::INFO, "FileLogger attached: Logs/app.log");
	} else {
		engine.multiLogger->Log(LogLevel::WARN, "FileLogger open failed: Logs/app.log");
	}

	// ===============================
	// シーンマネージャ初期化 & 最初のシーン
	// ===============================
	SceneManager sceneMgr;
	sceneMgr.Initialize(engine);
	sceneMgr.ChangeSceneT<GameScene>();  // 最初のシーンを予約

	// ===============================
	// メインループ
	// ===============================
	using clock = std::chrono::high_resolution_clock;
	auto prev = clock::now();

	const float kDtClampMin = 1.0f / 240.0f; // 低すぎるdtの下限
	const float kDtClampMax = 1.0f / 15.0f;  // スパイク抑制の上限

	bool running = true;
	while (running) {
		// Windows メッセージ処理（true が返ったら終了）
		if (winApp->ProcessMessage()) {
			break;
		}

		// --- dt 計測（秒） ---
		auto now = clock::now();
		float dt = std::chrono::duration<float>(now - prev).count();
		prev = now;
		if (dt < kDtClampMin) dt = kDtClampMin;
		if (dt > kDtClampMax) dt = kDtClampMax;

		// --- 更新 ---
		sceneMgr.Update(dt);

		// --- 描画 ---
		const float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
		dx->PreDraw(clearColor);

		RenderContext rc{};
		rc.commandList = dx->GetCommandList();

		sceneMgr.Draw(rc);

		dx->PostDraw();
	}

	// ===============================
	// 終了処理
	// ===============================
	sceneMgr.Finalize();       // 現在シーンのFinalize
	dx->Finalize();            // D3D12 後片付け
	winApp->Finalize();        // ウィンドウ破棄

	return 0;
}
