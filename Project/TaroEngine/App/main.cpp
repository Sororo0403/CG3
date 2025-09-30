#include "DirectXCommon.h"
#include "WinApp.h"
#include "SpriteCommon.h"
#include "ShaderCompiler.h"
#include "Sprite.h"

// Windowsアプリのエントリポイント
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    // ===============================
    // アプリ基盤の初期化
    // ===============================
    auto *winApp = new WinApp();
    winApp->Initialize(); // ウィンドウ生成 & 表示

    // ===============================
    // DirectX12 の初期化
    // ===============================
    auto *dx = new DirectXCommon();
    dx->Initialize(winApp);

    // ===============================
    // ウィンドウサイズ変更時の処理
    // ===============================
    winApp->SetOnResize([dx](uint32_t w, uint32_t h, UINT state) {
        if (state == SIZE_MINIMIZED) // 最小化時は無視
            return;
        dx->Resize(w, h); // バックバッファ等の再生成
        });

    // ===============================
    // Sprite 共通パイプラインの初期化
    // ===============================
    ShaderCompiler compiler;
    compiler.Initialize();

    SpriteCommon spriteCommon;
    spriteCommon.Initialize(dx->GetDevice());

    SpriteCommon::PipelineFormats formats{};
    formats.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    formats.dsvFormat = DXGI_FORMAT_UNKNOWN;
    formats.numRenderTargets = 1;

    spriteCommon.CreateGraphicsPipeline(
        compiler,
        dx->GetDevice(),
        L"Resources/Shaders/SpriteVS.hlsl",
        L"Resources/Shaders/SpritePS.hlsl",
        formats,
        L"main",
        L"main");

    // ===============================
    // Sprite オブジェクト生成
    // ===============================
    Sprite sprite;
    sprite.Initialize(dx->GetDevice());

    // ===============================
    // メインループ
    // ===============================
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (winApp->ProcessMessage()) // Windowsメッセージ処理
            break;

        // 更新
        sprite.Update();

        // 描画開始
        float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
        dx->PreDraw(clearColor);

        // Sprite 描画設定
        auto *cmdList = dx->GetCommandList();
        spriteCommon.ApplyCommonDrawSettings(cmdList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Sprite 描画
        sprite.Draw(cmdList);

        // 描画終了 & Present
        dx->PostDraw();
    }

    // ===============================
    // 終了処理
    // ===============================
    dx->Finalize();
    winApp->Finalize();
    delete dx;
    delete winApp;
    return 0;
}
