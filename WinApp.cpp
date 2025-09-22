#include "WinApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

/// <summary>
/// ImGui 側の Win32 メッセージ処理関数。
/// </summary>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

void WinApp::Initialize() {
  // COM ライブラリ初期化（マルチスレッド）
  HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);
  assert(SUCCEEDED(hr));

  // ウィンドウクラス設定
  wc.lpfnWndProc = WinApp::WindowProc;
  wc.lpszClassName = L"CG2WindowClass";
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClass(&wc);

  // クライアント領域サイズ調整
  RECT wrc = {0, 0, kClientWidth, kClientHeight};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

  // ウィンドウ生成
  hwnd =
      CreateWindow(wc.lpszClassName, L"CG2", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                   CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top,
                   nullptr, nullptr, wc.hInstance, nullptr);

  // 表示
  ShowWindow(hwnd, SW_SHOW);
}

void WinApp::Update() {
  // 今は特に処理なし
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
  // ImGui にメッセージを渡す
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }

  switch (msg) {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
