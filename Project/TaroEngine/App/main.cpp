#include "DirectXCommon.h"
#include "WinApp.h"

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
  // メインループ
  // ===============================
  MSG msg{};
  while (msg.message != WM_QUIT) {
    if (winApp->ProcessMessage()) // Windowsメッセージ処理
      break;

    // 描画開始
    float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
    dx->PreDraw(clearColor);

    // TODO: ゲームの描画処理をここに追加する

    // 描画終了 & Present
    dx->PostDraw();
  }

  // ===============================
  // 終了処理
  // ===============================
  dx->Finalize();
  delete dx;
  delete winApp;
  return 0;
}
