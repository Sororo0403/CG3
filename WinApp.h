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
  /// 毎フレーム呼び出される更新処理。
  /// （必要ならメッセージポンプなどをここに追加する）
  /// </summary>
  void Update();

  /// <summary>
  /// 終了処理。ウィンドウ破棄や COM の後始末を行う。
  /// </summary>
  void Finalize();

  // ==== getter ====

  /// <summary>
  /// ウィンドウハンドルを取得する。
  /// </summary>
  HWND GetHwnd() const { return hwnd_; }

  /// <summary>
  ///ウィンドウクラスに登録された HINSTANCE を取得する。
  /// </summary>
  HINSTANCE GetHInstance() const { return wc_.hInstance; }

  /// <summary>
  /// ウィンドウプロシージャ。
  /// Windows のメッセージを受け取り、適切に処理する。
  /// ImGui のメッセージハンドラへも転送する。
  /// </summary>
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam,
                                     LPARAM lparam);

private:
  // Window
  HWND hwnd_ = nullptr;
  WNDCLASS wc_{};
};
