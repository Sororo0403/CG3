#pragma once
#include <dinput.h>
#include <wrl.h>

class Input {
public:
  // 初期化（DirectInput + キーボード）
  void Initialize(HINSTANCE hInstance, HWND hwnd);
  // 毎フレーム更新
  void Update();

  // 押されている間
  bool IsKeyDown(BYTE dik) const { return (m_now[dik] & 0x80) != 0; }
  // 離された瞬間
  bool IsKeyReleased(BYTE dik) const {
    return (m_prev[dik] & 0x80) && !(m_now[dik] & 0x80);
  }
  // 押した瞬間
  bool IsKeyPressed(BYTE dik) const {
    return !(m_prev[dik] & 0x80) && (m_now[dik] & 0x80);
  }

private:
  Microsoft::WRL::ComPtr<IDirectInput8> m_di;
  Microsoft::WRL::ComPtr<IDirectInputDevice8> m_keyboard;
  BYTE m_now[256] = {};
  BYTE m_prev[256] = {};
};
