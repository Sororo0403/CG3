#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(WinApp *winApp) {
  // WinApp から借りたインスタンスを記録
  winApp_ = winApp;

  // DirectInput の生成
  HRESULT hr = DirectInput8Create(
      winApp_->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
      reinterpret_cast<void **>(di_.GetAddressOf()), nullptr);
  assert(SUCCEEDED(hr));

  // キーボードデバイスを作成
  hr = di_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
  assert(SUCCEEDED(hr));

  // データフォーマットをキーボード用に設定
  hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
  assert(SUCCEEDED(hr));

  // 協調モードを設定（前面・非排他・Winキー無効）
  hr = keyboard_->SetCooperativeLevel(winApp_->GetHwnd(),
                                      DISCL_FOREGROUND | DISCL_NONEXCLUSIVE |
                                          DISCL_NOWINKEY);
  assert(SUCCEEDED(hr));
}

void Input::Update() {
  // 前フレームの状態をコピー
  std::memcpy(prev_, now_, sizeof(now_));

  // デバイスを取得（フォーカスを失っていた場合の再取得を考慮）
  if (FAILED(keyboard_->Acquire()))
    return;

  // 現在のキー状態を取得
  keyboard_->GetDeviceState(sizeof(now_), now_);
}
