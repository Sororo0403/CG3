#include "DirectXCommon.h"
#include "WinApp.h"
#include "SpriteCommon.h"
#include "ShaderCompiler.h"
#include "EngineContext.h"
#include "GameScene.h"

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    // --- 基盤初期化 ---
    auto *winApp = new WinApp();
    winApp->Initialize();

    auto *dx = new DirectXCommon();
    dx->Initialize(winApp);

    winApp->SetOnResize([dx](uint32_t w, uint32_t h, UINT state) {
        if (state == SIZE_MINIMIZED) return;
        dx->Resize(w, h);
        });

    // --- Sprite 共通パイプライン（アプリ全体で1回だけ）---
    ShaderCompiler compiler;
    compiler.Initialize();

    auto *spriteCommon = new SpriteCommon();
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

    // --- DI: EngineContext を用意 ---
    EngineContext engine{};
    engine.dx = dx;
    engine.device = dx->GetDevice();
    engine.spriteCommon = spriteCommon;

    // --- 単独シーンを作って開始 ---
    GameScene scene;
    scene.Initialize(engine);

    // --- メインループ ---
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (winApp->ProcessMessage()) break;

        scene.Update(1.0f / 60.0f); // 必要なら dt 計測に差し替え

        float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
        dx->PreDraw(clearColor);

        RenderContext rc{};
        rc.cmdList = dx->GetCommandList();

        scene.Draw(engine, rc);

        dx->PostDraw();
    }

    // --- 終了 ---
    scene.Finalize();
    dx->Finalize();
    winApp->Finalize();

    delete spriteCommon;
    delete dx;
    delete winApp;
    return 0;
}
