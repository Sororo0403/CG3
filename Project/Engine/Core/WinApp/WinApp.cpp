#include "WinApp.h"
#include "Logger/LoggerManager.h"
#include "String/StringUtil.h"
#include <cassert>

WinApp::~WinApp() {
  LOG_INFO("WinApp destructor called");

  if (hwnd_) {
    LOG_INFO("Destroying window: hwnd={}", reinterpret_cast<uintptr_t>(hwnd_));

    if (!DestroyWindow(hwnd_)) {
      LOG_ERROR("DestroyWindow failed: GetLastError={}", GetLastError());
    }

    hwnd_ = nullptr;
  }

  if (wc_.lpszClassName) {
    LOG_INFO("Unregistering window class: {}",
             StringUtil::ToString(wc_.lpszClassName));

    if (!UnregisterClass(wc_.lpszClassName, wc_.hInstance)) {
      LOG_ERROR("UnregisterClass failed: GetLastError={}", GetLastError());
    }
  }
}

void WinApp::Initialize(LONG width, LONG height, const std::wstring &title) {
  LOG_INFO("WinApp::Initialize start: size={}x{}", width, height);

  width_ = width;
  height_ = height;

  // WindowClass 設定
  wc_.lpfnWndProc = WinApp::WindowProc;
  wc_.lpszClassName = L"WindowClass";
  wc_.hInstance = GetModuleHandle(nullptr);
  wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

  LOG_INFO("RegisterClass: className=WindowClass");

  ATOM atom = RegisterClass(&wc_);
  if (!atom) {
    LOG_ERROR("RegisterClass failed: GetLastError={}", GetLastError());
    assert(false);
  }

  // クライアント領域調整
  RECT wrc{0, 0, width_, height_};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

  LOG_INFO("Adjusted window rect: width={}, height={}", wrc.right - wrc.left,
           wrc.bottom - wrc.top);

  // Window 作成
  hwnd_ = CreateWindow(wc_.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                       wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance,
                       nullptr);

  if (!hwnd_) {
    LOG_ERROR("CreateWindow failed: GetLastError={}", GetLastError());
    assert(false);
  }

  LOG_INFO("Window created successfully: hwnd={}",
           reinterpret_cast<uintptr_t>(hwnd_));

  ShowWindow(hwnd_, SW_SHOW);
  LOG_INFO("Window shown");

  LOG_INFO("WinApp::Initialize completed");
}

bool WinApp::ProcessMessage() {
  MSG msg{};
  if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

    LOG_DEBUG("Message received: msg=0x{:X}", msg.message);

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

  case WM_CLOSE:
    LOG_INFO("WM_CLOSE received");
    break;

  case WM_SIZE: {
    int w = LOWORD(lparam);
    int h = HIWORD(lparam);

    LOG_INFO("WM_SIZE: resized to {}x{}", w, h);
    break;
  }

  case WM_ACTIVATE:
    LOG_DEBUG("WM_ACTIVATE: wparam={}", wparam);
    break;

  default:
    LOG_DEBUG("WindowProc: msg=0x{:X}, wparam={}, lparam={}", msg, wparam,
              lparam);
    break;
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
