#include "WinApp.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

#include <cassert>

#include "Logger/Logger.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

WinApp::~WinApp() {
  LOG_INFO("WinApp destructor called");

  if (hwnd_) {
    if (!DestroyWindow(hwnd_)) {
      LOG_ERROR("DestroyWindow failed");
    }
    hwnd_ = nullptr;
  }

  if (wc_.lpszClassName) {
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

  ATOM atom = RegisterClass(&wc_);
  if (!atom) {
    LOG_ERROR("RegisterClass failed");
    assert(false);
  }

  RECT wrc{0, 0, width_, height_};
  AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, FALSE);

  hwnd_ = CreateWindow(wc_.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW,
                       CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                       wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance,
                       nullptr);

  if (!hwnd_) {
    LOG_ERROR("CreateWindow failed");
    assert(false);
  }

  ShowWindow(hwnd_, SW_SHOW);
  LOG_INFO("WinApp::Initialize completed");
}

bool WinApp::ProcessMessage() {
  MSG msg{};
  if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {

    TranslateMessage(&msg);
    DispatchMessage(&msg);

    if (msg.message == WM_QUIT) {
      LOG_INFO("WM_QUIT received");
      return true;
    }
  }
  return false;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                    LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
    return true;
  }

  switch (msg) {

  case WM_DESTROY:
    LOG_INFO("WM_DESTROY");
    PostQuitMessage(0);
    return 0;

  case WM_CLOSE:
    LOG_INFO("WM_CLOSE");
    break;

  default:
    break;
  }

  return DefWindowProc(hwnd, msg, wparam, lparam);
}
