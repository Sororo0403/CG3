#include "WinApp.h"
#include "Logger/LoggerManager.h"
#include <cassert>

WinApp::~WinApp() {
  LOG_INFO("WinApp destructor called");

  if (hwnd_) {
    LOG_INFO("Destroying window");
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }

  if (wc_.lpszClassName) {
    LOG_INFO("Unregistering window class");
    UnregisterClass(wc_.lpszClassName, wc_.hInstance);
  }
}

void WinApp::Initialize(LONG width, LONG height, const std::wstring &title) {
  width_ = width;
  height_ = height;

  LOG_INFO("WinApp::Initialize start");

  // WindowClass 設定
  wc_.lpfnWndProc = WinApp::WindowProc;
  wc_.lpszClassName = L"WindowClass";
  wc_.hInstance = GetModuleHandle(nullptr);
  wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

  LOG_INFO("RegisterClass called");
  RegisterClass(&wc_);

  // クライアント領域調整
  RECT wrc{0, 0, width_, height_};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

  // Window 作成
  hwnd_ = CreateWindow(wc_.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                       wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance,
                       nullptr);

  if (!hwnd_) {
    LOG_ERROR("Failed to create window");
  } else {
    LOG_INFO("Window created successfully");
  }

  assert(hwnd_ != nullptr);

  ShowWindow(hwnd_, SW_SHOW);
  LOG_INFO("Window shown");
}

bool WinApp::ProcessMessage() {
  MSG msg{};
  if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    LOG_DEBUG("PeekMessage received");

    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (msg.message == WM_QUIT) {
      LOG_INFO("Received WM_QUIT");
      return true;
    }
  }

  return false;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {

  switch (msg) {
  case WM_DESTROY:
    LOG_INFO("WM_DESTROY received");
    PostQuitMessage(0);
    return 0;

  case WM_SIZE:
    LOG_DEBUG("WM_SIZE window resized");
    break;

  case WM_CLOSE:
    LOG_INFO("WM_CLOSE received");
    break;
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
