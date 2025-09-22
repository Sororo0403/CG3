#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
  HRESULT hr =
      DirectInput8Create(hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
                         (void **)m_di.GetAddressOf(), nullptr);
  assert(SUCCEEDED(hr));

  hr = m_di->CreateDevice(GUID_SysKeyboard, m_keyboard.GetAddressOf(), nullptr);
  assert(SUCCEEDED(hr));

  hr = m_keyboard->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));

  hr = m_keyboard->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
  assert(SUCCEEDED(hr));
}

void Input::Update() {
  // 前フレーム保存
  std::memcpy(m_prev, m_now, sizeof(m_now));

  // 取得（フォーカス外などで失敗したら何もしない）
  if (FAILED(m_keyboard->Acquire()))
    return;
  m_keyboard->GetDeviceState(sizeof(m_now), m_now);
}
