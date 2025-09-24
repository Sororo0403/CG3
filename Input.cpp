#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(WinApp *winApp) {
  // 借りてきたインスタンスを記録
  winApp_ = winApp;

  HRESULT hr = DirectInput8Create(
      winApp_->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
      reinterpret_cast<void **>(di_.GetAddressOf()), nullptr);
  assert(SUCCEEDED(hr));

  hr = di_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
  assert(SUCCEEDED(hr));

  hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));

  hr = keyboard_->SetCooperativeLevel(winApp_->GetHwnd(),
                                      DISCL_FOREGROUND | DISCL_NONEXCLUSIVE |
                                          DISCL_NOWINKEY);
  assert(SUCCEEDED(hr));
}

void Input::Update() {
  std::memcpy(prev_, now_, sizeof(now_));

  if (FAILED(keyboard_->Acquire()))
    return;

  keyboard_->GetDeviceState(sizeof(now_), now_);
}
