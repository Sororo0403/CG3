#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>

/// <summary>
/// DirectInput を使ったキーボード入力管理クラス。
/// 毎フレームのキー状態を保持し、押下・離し・トリガー判定を提供する。
/// </summary>
class Input {
private:
  /// <summary>
  /// using namespace の代わりに、ComPtr
  /// を短く書けるようにするエイリアステンプレート。 Microsoft::WRL::ComPtr<T>
  /// を「ComPtr<T>」とだけ書ける。
  /// </summary>
  template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
  /// <summary>
  /// キー押下状態のビットマスク。
  /// DirectInput では 0x80 が押下を意味する。
  /// </summary>
  static constexpr BYTE KEY_PRESSED_MASK = 0x80;

  /// <summary>
  /// DirectInput の初期化とキーボードデバイスの生成を行う。
  /// </summary>
  /// <param name="hInstance">アプリケーションインスタンスハンドル。</param>
  /// <param name="hwnd">入力を関連付けるウィンドウのハンドル。</param>
  void Initialize(HINSTANCE hInstance, HWND hwnd);

  /// <summary>
  /// 毎フレーム呼び出して、前フレームと現在フレームのキー状態を更新する。
  /// </summary>
  void Update();

  /// <summary>
  /// 指定したキーが押され続けているかを返す。
  /// </summary>
  /// <param name="dik">DirectInput の DIK_～ 定数。</param>
  /// <returns>true = 押されている / false = 押されていない。</returns>
  bool IsKeyDown(BYTE dik) const {
    return (m_now[dik] & KEY_PRESSED_MASK) != 0;
  }

  /// <summary>
  /// 指定したキーが「離された瞬間」かどうかを返す。
  /// 前フレームで押されていて、現在は押されていない場合に true。
  /// </summary>
  /// <param name="dik">DirectInput の DIK_～ 定数。</param>
  /// <returns>true = 離された瞬間 / false = それ以外。</returns>
  bool IsKeyReleased(BYTE dik) const {
    return (m_prev[dik] & KEY_PRESSED_MASK) && !(m_now[dik] & KEY_PRESSED_MASK);
  }

  /// <summary>
  /// 指定したキーが「押された瞬間」かどうかを返す。
  /// 前フレームで押されておらず、現在は押されている場合に true。
  /// </summary>
  /// <param name="dik">DirectInput の DIK_～ 定数。</param>
  /// <returns>true = 押した瞬間 / false = それ以外。</returns>
  bool IsKeyPressed(BYTE dik) const {
    return !(m_prev[dik] & KEY_PRESSED_MASK) && (m_now[dik] & KEY_PRESSED_MASK);
  }

private:
  ComPtr<IDirectInput8> di_;             ///< DirectInput 本体
  ComPtr<IDirectInputDevice8> keyboard_; ///< キーボードデバイス
  BYTE m_now[256] = {};                  ///< 現在のキー状態
  BYTE m_prev[256] = {};                 ///< 前フレームのキー状態
};
