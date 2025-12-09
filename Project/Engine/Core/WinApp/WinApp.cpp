#include "WinApp.h"
#include "Logger/Logger.h"
#include <cassert>

WinApp::~WinApp() {
  LOG_INFO("WinApp destructor called");

  if (hwnd_) {
    LOG_INFO("Destroying window");

    if (!DestroyWindow(hwnd_)) {
      LOG_ERROR("DestroyWindow failed");
    }

    hwnd_ = nullptr;
  }

  if (wc_.lpszClassName) {
    LOG_INFO("Unregistering window class");

    if (!UnregisterClass(wc_.lpszClassName, wc_.hInstance)) {
      LOG_ERROR("UnregisterClass failed");
    }
  }
}

void WinApp::Initialize(LONG width, LONG height, const std::wstring &title) {
  LOG_INFO("WinApp::Initialize start");

  width_ = width;
  height_ = height;

  wc_.lpfnWndProc = WinApp::WindowProc;
  wc_.lpszClassName = L"WindowClass";
  wc_.hInstance = GetModuleHandle(nullptr);
  wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

  LOG_INFO("RegisterClass called");

  ATOM atom = RegisterClass(&wc_);
  if (!atom) {
    LOG_ERROR("RegisterClass failed");
    assert(false);
  }

  RECT wrc{0, 0, width_, height_};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

  LOG_INFO("Adjusted window rect");

  hwnd_ = CreateWindow(wc_.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                       wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance,
                       nullptr);

  if (!hwnd_) {
    LOG_ERROR("CreateWindow failed");
    assert(false);
  }

  LOG_INFO("Window created");

  ShowWindow(hwnd_, SW_SHOW);
  LOG_INFO("Window shown");

  LOG_INFO("WinApp::Initialize completed");
}

bool WinApp::ProcessMessage() {
  MSG msg{};
  if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

    LOG_DEBUG("Message received");

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

  case WM_SIZE:
    LOG_INFO("WM_SIZE received");
    break;

  case WM_ACTIVATE:
    LOG_DEBUG("WM_ACTIVATE received");
    break;

  default:
    LOG_DEBUG("WindowProc: other message");
    break;
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
