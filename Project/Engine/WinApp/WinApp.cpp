#include "WinApp.h"

#include <shellapi.h>
#include <cassert>

#include "Logger/Logger.h"
#include "Texture/TextureDropQueue.h"
#include "String/StringUtil.h"

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                              WPARAM wParam, LPARAM lParam);

WinApp::WinApp(LONG width, LONG height, const std::wstring &title,
               TextureDropQueue *textureDropQueue)
    : width_(width), height_(height), title_(title),
      textureDropQueue_(textureDropQueue) {
    assert(width_ > 0);
    assert(height_ > 0);
}

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

void WinApp::Initialize() {
    LOG_INFO("WinApp::Initialize start");

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

    hwnd_ = CreateWindow(wc_.lpszClassName, title_.c_str(), WS_OVERLAPPEDWINDOW,
                         CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left,
                         wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance,
                         this);

    if (!hwnd_) {
        LOG_ERROR("CreateWindow failed");
        assert(false);
    }

    ShowWindow(hwnd_, SW_SHOW);

    DragAcceptFiles(hwnd_, TRUE);

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
    WinApp *app = nullptr;

    if (msg == WM_NCCREATE) {
        auto *cs = reinterpret_cast<CREATESTRUCT *>(lparam);
        app = reinterpret_cast<WinApp *>(cs->lpCreateParams);

        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    } else {
        app = reinterpret_cast<WinApp *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
        return true;
    }

    switch (msg) {

case WM_DROPFILES: {
        if (!app || !app->textureDropQueue_) {
            DragFinish(reinterpret_cast<HDROP>(wparam));
            return 0;
        }

        HDROP hDrop = reinterpret_cast<HDROP>(wparam);
        const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);

        for (UINT i = 0; i < count; ++i) {
            // 長さ取得（終端含まない）
            const UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
            if (len == 0) {
                continue;
            }

            // len + 1 分確保（終端用）
            std::wstring wpath;
            wpath.resize(len + 1);

            // 実データ取得
            const UINT written = DragQueryFileW(
                hDrop, i, wpath.data(), static_cast<UINT>(wpath.size()));

            if (written == 0) {
                continue;
            }

            // 終端を落とす
            wpath.resize(written);

            // UTF-8 へ変換してキューへ
            app->textureDropQueue_->Push(StringUtil::UTF16ToUTF8(wpath));
        }

        DragFinish(hDrop);
        return 0;
    }

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
