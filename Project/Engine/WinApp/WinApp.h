#pragma once

#include <windows.h>
#include <string>

class TextureDropQueue;

class WinApp {
  public:
    WinApp(LONG width, LONG height, const std::wstring &title,
           TextureDropQueue *textureDropQueue);

    /// <summary>
    /// デストラクタ
    /// </summary>
    ~WinApp();

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize();

    /// <summary>
    /// メッセージポンプを1回分実行する
    /// </summary>
    /// <returns>WM_QUITがきたら true</returns>
    bool ProcessMessage();

    // Getter
    HWND GetHwnd() const {
        return hwnd_;
    }
    HINSTANCE GetHInstance() const {
        return wc_.hInstance;
    }
    LONG GetWidth() const {
        return width_;
    }
    LONG GetHeight() const {
        return height_;
    }

  private:
    /// <summary>
    /// ウィンドウプロシージャ
    /// </summary>
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                       LPARAM lparam);

  private:
    HWND hwnd_;
    WNDCLASS wc_{};

    LONG width_;
    LONG height_;
    std::wstring title_;

    TextureDropQueue *textureDropQueue_ = nullptr;
};
