#include "D3DResourceLeakChecker.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "WinApp.h"
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
  // Windowsアプリ初期化
  WinApp *winApp = new WinApp();
  winApp->Initialize();

  // DirectX 初期化
  DirectXCommon *dxCommon = new DirectXCommon();
  dxCommon->Initialize(winApp);

  // 入力初期化
  Input *input = new Input();
  input->Initialize(winApp);

  // メインループ
  MSG msg{};
  while (msg.message != WM_QUIT) {
    if (winApp->ProcessMessage())
      break;

    input->Update();

    // 描画開始
    float clearColor[] = {0.1f, 0.25f, 0.5f, 1.0f};
    dxCommon->PreDraw(clearColor);

    // 描画終了
    dxCommon->PostDraw();
  }

  // 終了処理
  delete input;
  dxCommon->Finalize();
  delete dxCommon;
  winApp->Finalize();
  delete winApp;

  D3DResourceLeakChecker leakChecker;

  return 0;
}
