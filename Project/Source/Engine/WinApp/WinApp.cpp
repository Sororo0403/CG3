#include "WinApp.h"
#include <cassert>

WinApp::~WinApp() {
	if (hwnd_) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}

	if (wc_.lpszClassName) {
		UnregisterClass(wc_.lpszClassName, wc_.hInstance);
	}
}

void WinApp::Initialize(LONG clientWidth, LONG clientHeight, const std::wstring &windowTitle) {
	// クライアント領域
	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	// ウィンドウクラス登録
	wc_.lpfnWndProc = WinApp::WindowProc;
	wc_.lpszClassName = L"WindowClass";
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	RegisterClass(&wc_);

	// クライアント領域を指定の大きさに調整
	RECT wrc{0, 0, clientWidth, clientHeight};
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウ生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		windowTitle.c_str(),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr
	);

	assert(hwnd_ != nullptr);

	// 表示
	ShowWindow(hwnd_, SW_SHOW);
}

bool WinApp::ProcessMessage() {
	MSG msg{};
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT) {
			return true;
		}
	}
	return false;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}
}
