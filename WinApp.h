#pragma once
#include <cstdint>
#include <windows.h>

/// <summary>
/// Windows アプリケーションの基盤クラス。
/// ウィンドウの生成、ハンドル取得、メッセージ処理などを担当する。
/// </summary>
class WinApp {
public:
  // ウィンドウのクライアント領域
  static const int32_t kClientWidth = 1280;
  static const int32_t kClientHeight = 720;

public:
  /// <summary>
  /// アプリケーションの初期化を行い、ウィンドウを生成・表示する。
  /// </summary>
  void Initialize();

  /// <summary>
  /// 終了処理。ウィンドウ破棄や COM の後始末を行う。
  /// </summary>
  void Finalize();

  /// <summary>
  /// ウィンドウプロシージャ。Win32 メッセージを受け取り処理します。
  /// 必要に応じて ImGui へも転送し、未処理のものは DefWindowProc に委譲します。
  /// </summary>
  /// <param name="hwnd">対象ウィンドウのハンドル。</param>
  /// <param name="msg">メッセージ ID（WM_～）。</param>
  /// <param name="wparam">追加パラメータ（WPARAM）。</param>
  /// <param name="lparam">追加パラメータ（LPARAM）。</param>
  /// <returns>処理結果の LRESULT。</returns>
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

  /// <summary>
  /// メッセージポンプを1回分実行
  /// </summary>
  /// <returns> WM_QUITを受信した場合はtrue。それ以外はfalse。</returns>
  bool ProcessMessage();

  // ==== getter ====

  /// <summary>
  /// このアプリのウィンドウのハンドルを返します。
  /// </summary>
  /// <returns>作成済みウィンドウの HWND。</returns>
  HWND GetHwnd() const { return hwnd_; }

  /// <summary>
  /// インスタンスハンドルを返します。
  /// </summary>
  /// <returns>アプリケーションの HINSTANCE。</returns>
  HINSTANCE GetHInstance() const { return wc_.hInstance; }

private:
  // Window
  HWND hwnd_ = nullptr;
  WNDCLASS wc_{};
};
