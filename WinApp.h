#pragma once
#include <cstdint>
#include <functional>
#include <windows.h>

/// <summary>
/// Windows アプリケーション基盤クラス。<br/>
/// ウィンドウの生成、メッセージ処理、サイズ変更コールバックなどを管理する。
/// </summary>
class WinApp {
public:
  // ===============================
  // 定数
  // ===============================

  // クライアント領域のデフォルト幅（初回のウィンドウ作成用）
  static const int32_t kClientWidth = 1280;

  // クライアント領域のデフォルト高さ（初回のウィンドウ作成用）
  static const int32_t kClientHeight = 720;

public:
  // ===============================
  // 基本処理
  // ===============================

  /// <summary>
  /// ウィンドウを生成・表示し、WinApp を初期化する。
  /// </summary>
  void Initialize();

  /// <summary>
  /// メッセージポンプを1回分実行する。<br/>
  /// WM_QUIT が来た場合は true を返す。
  /// </summary>
  bool ProcessMessage();

  // ===============================
  // コールバック設定
  // ===============================

  /// <summary>
  /// サイズ変更時に呼び出されるコールバックを登録する。<br/>
  /// state には WM_SIZE の wParam（SIZE_MINIMIZED / SIZE_RESTORED /
  /// SIZE_MAXIMIZED）が渡される。
  /// </summary>
  void SetOnResize(
      std::function<void(uint32_t width, uint32_t height, UINT state)> cb) {
    onResize_ = std::move(cb);
  }

  // ===============================
  // Getter
  // ===============================

  /// <summary>
  /// ウィンドウハンドルを取得する。
  /// </summary>
  HWND GetHwnd() const { return hwnd_; }

  /// <summary>
  /// インスタンスハンドルを取得する。
  /// </summary>
  HINSTANCE GetHInstance() const { return wc_.hInstance; }

  // ===============================
  // WindowProc
  // ===============================

  /// <summary>
  /// ウィンドウプロシージャ。<br/>
  /// Windows メッセージを受け取り処理する。<br/>
  /// WM_SIZE 時に onResize_ が設定されていればコールバックする。
  /// </summary>
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

private:
  // ===============================
  // ヘルパ
  // ===============================

  /// <summary>
  /// HWND にぶら下げられた自分自身のポインタを取得する。
  /// </summary>
  static WinApp *FromHwnd(HWND hwnd) {
    return reinterpret_cast<WinApp *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  }

private:
  // ===============================
  // メンバ変数
  // ===============================

  HWND hwnd_ = nullptr; // ウィンドウハンドル
  WNDCLASS wc_{};       // ウィンドウクラス構造体
  std::function<void(uint32_t, uint32_t, UINT)>
      onResize_; // サイズ変更コールバック
};
