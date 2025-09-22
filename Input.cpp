#include "Input.h"
#include <cassert>
#include <cstring>
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxgi.lib")

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
  HRESULT hr = DirectInput8Create(
      hInstance, DIRECTINPUT_VERSION, IID_IDirectInput8,
      reinterpret_cast<void **>(di_.GetAddressOf()), nullptr);
  assert(SUCCEEDED(hr));

  hr = di_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
  assert(SUCCEEDED(hr));

  hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));

  hr = keyboard_->SetCooperativeLevel(
      hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
  assert(SUCCEEDED(hr));
}

void Input::Update() {
  // 前フレーム保存
  std::memcpy(m_prev, m_now, sizeof(m_now));

  // キーボードの状態取得（フォーカス外などで失敗したら無視）
  if (FAILED(keyboard_->Acquire()))
    return;

  keyboard_->GetDeviceState(sizeof(m_now), m_now);
}
