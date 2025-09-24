#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include "WinApp.h"
#include <dinput.h>
#include <wrl.h>

/// <summary>
/// DirectInput を使ったキーボード入力管理クラス。
/// 毎フレームのキー状態を保持し、押下・離し・トリガー判定を提供する。
/// </summary>
class Input {
private:
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
  static constexpr BYTE KEY_PRESSED_MASK = 0x80;

  /// <summary>
  /// DirectInput の初期化とキーボードデバイスの生成を行う。
  /// WinApp クラスを借りて、必要な HINSTANCE / HWND を取得する。
  /// </summary>
  void Initialize(WinApp *winApp);

  /// <summary>
  /// 毎フレーム呼び出して、前フレームと現在フレームのキー状態を更新する。
  /// </summary>
  void Update();

  /// <summary>
  /// 指定したキーが押され続けているかを返す。
  /// </summary>
  bool IsKeyDown(BYTE dik) const { return (now_[dik] & KEY_PRESSED_MASK) != 0; }

  /// <summary>
  /// 指定したキーが「離された瞬間」かを返す。
  /// </summary>
  bool IsKeyReleased(BYTE dik) const {
    return (prev_[dik] & KEY_PRESSED_MASK) && !(now_[dik] & KEY_PRESSED_MASK);
  }

  /// <summary>
  /// 指定したキーが「押された瞬間」かを返す。
  /// </summary>
  bool IsKeyPressed(BYTE dik) const {
    return !(prev_[dik] & KEY_PRESSED_MASK) && (now_[dik] & KEY_PRESSED_MASK);
  }

private:
  // WinApp
  WinApp *winApp_ = nullptr;

  // DirectInput
  ComPtr<IDirectInput8> di_;
  ComPtr<IDirectInputDevice8> keyboard_;
  BYTE now_[256] = {};
  BYTE prev_[256] = {};
};
