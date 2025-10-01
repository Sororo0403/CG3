#include "WinApp.h"
#include <cassert>
#include <mmsystem.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void WinApp::Initialize() {
	// 高精度タイマー開始（1ms単位にする）
	timeBeginPeriod(1);

	// ===============================
	// ウィンドウクラス登録
	// ===============================
	wc_.lpfnWndProc = WinApp::WindowProc;  // メインのメッセージ処理関数
	wc_.lpszClassName = L"CG3WindowClass"; // クラス名
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc_);

	// ===============================
	// クライアント領域サイズをフレーム込みに調整
	// ===============================
	RECT wrc{0, 0, kClientWidth, kClientHeight};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

	// ===============================
	// ウィンドウ生成
	// 重要：lpParam に this を渡す
	// → WM_NCCREATE で回収して GWLP_USERDATA に保存する
	// ===============================
	hwnd_ =
		CreateWindow(wc_.lpszClassName, L"CG3", WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
			wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance, this);

	// 表示
	ShowWindow(hwnd_, SW_SHOW);
}

void WinApp::Finalize() {
	// ウィンドウを閉じる
	if (hwnd_) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}

	// クラス登録を解除
	if (wc_.lpszClassName && wc_.hInstance) {
		UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	}

	// 1ms タイマー精度を元に戻す
	timeEndPeriod(1);
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	// 非ブロッキングでメッセージを取得
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg); // 仮想キー → 文字メッセージ変換
		DispatchMessage(&msg);  // WindowProc にディスパッチ
		if (msg.message == WM_QUIT) {
			return true; // 終了リクエスト
		}
	}
	return false; // まだ続行
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	// ImGui のウィンドウプロシージャも呼ぶ
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}

	switch (msg) {
	case WM_NCCREATE: {
		// CreateWindow の lpParam から this を取得し、HWND に紐づけ
		auto cs = reinterpret_cast<CREATESTRUCT *>(lparam);
		auto self = reinterpret_cast<WinApp *>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	case WM_SIZE: {
		// サイズ変更通知
		auto self = WinApp::FromHwnd(hwnd);
		if (!self)
			break;

		const UINT state = static_cast<UINT>(wparam);
		const UINT newW = LOWORD(lparam);
		const UINT newH = HIWORD(lparam);

		// コールバックが設定されていれば呼ぶ（実際の処理は外側で）
		if (self->onResize_) {
			self->onResize_(newW, newH, state);
		}
		return 0;
	}

	case WM_DESTROY:
		// ウィンドウ破棄時 → アプリ終了
		PostQuitMessage(0);
		return 0;
	}

	// 既定の処理にフォールバック
	return DefWindowProc(hwnd, msg, wparam, lparam);
}
