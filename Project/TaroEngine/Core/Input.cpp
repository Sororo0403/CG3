#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(WinApp *winApp) {
    // WinApp から借りたインスタンスを保存
    winApp_ = winApp;

    // DirectInput オブジェクトを生成
    HRESULT hr = DirectInput8Create(
        winApp_->GetHInstance(), // アプリのインスタンス
        DIRECTINPUT_VERSION, // バージョン
        IID_IDirectInput8, // DirectInput8 の GUID
        reinterpret_cast<void **>(directInput_.GetAddressOf()), // ComPtr に格納
        nullptr);
    assert(SUCCEEDED(hr));

    // キーボードデバイスを作成
    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
    assert(SUCCEEDED(hr));

    // データフォーマットをキーボード用に設定
    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    // 協調モードを設定
    hr = keyboard_->SetCooperativeLevel(
        winApp_->GetHwnd(),
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update() {
    // 前フレームのキー状態を保存
    std::memcpy(prev_, now_, sizeof(now_));

    // 入力デバイスを取得（フォーカス喪失時は Acquire が必要）
    if (FAILED(keyboard_->Acquire())) {
        return; // フォーカスが戻るまで何もしない
    }

    // 今フレームのキー状態を取得
    keyboard_->GetDeviceState(sizeof(now_), now_);
}
