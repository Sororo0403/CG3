#include "Input.h"
#include <cassert>
#include <cstring>

bool Input::Initialize(HINSTANCE hInstance, HWND hwnd) noexcept {
    HRESULT hr = DirectInput8Create(
        hInstance,
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void **>(directInput_.GetAddressOf()),
        nullptr);
    if (FAILED(hr)) return false;

    // ---- Keyboard ----
    hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
    if (FAILED(hr)) return false;

    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    if (FAILED(hr)) return false;

    hr = keyboard_->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    if (FAILED(hr)) return false;

    // ---- Mouse ----
    hr = directInput_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
    if (FAILED(hr)) return false;

    // DIMOUSESTATE2 を使う（8ボタン＋高解像度ホイール対応）
    hr = mouse_->SetDataFormat(&c_dfDIMouse2);
    if (FAILED(hr)) return false;

    // ゲームでよく使うのは非排他・フォアグラウンド
    hr = mouse_->SetCooperativeLevel(
        hwnd,
        DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
    if (FAILED(hr)) return false;

    ResetStates();

    // 最初の Acquire を試みる
    TryAcquireKeyboard_();
    TryAcquireMouse_();
    return true;
}

void Input::Update() noexcept {
    // ---- Keyboard ----
    std::memcpy(prevKey_, nowKey_, sizeof(nowKey_));
    if (!TryAcquireKeyboard_()) {
        std::memset(nowKey_, 0, sizeof(nowKey_));
    } else {
        HRESULT hr = keyboard_->GetDeviceState(static_cast<DWORD>(sizeof(nowKey_)), nowKey_);
        if (FAILED(hr)) {
            std::memset(nowKey_, 0, sizeof(nowKey_));
            TryAcquireKeyboard_();
        }
    }

    // ---- Mouse ----
    mousePrev_ = mouseNow_; // DIMOUSESTATE2 は単純代入でOK
    std::memset(&mouseNow_, 0, sizeof(mouseNow_));
    if (TryAcquireMouse_()) {
        HRESULT hr = mouse_->GetDeviceState(static_cast<DWORD>(sizeof(mouseNow_)), &mouseNow_);
        if (FAILED(hr)) {
            std::memset(&mouseNow_, 0, sizeof(mouseNow_));
            TryAcquireMouse_();
        }
    }
}

void Input::Finalize() noexcept {
    if (keyboard_) keyboard_->Unacquire();
    if (mouse_)    mouse_->Unacquire();
    mouse_.Reset();
    keyboard_.Reset();
    directInput_.Reset();
    ResetStates();
}

void Input::ResetStates() noexcept {
    std::memset(nowKey_, 0, sizeof(nowKey_));
    std::memset(prevKey_, 0, sizeof(prevKey_));
    std::memset(&mouseNow_, 0, sizeof(mouseNow_));
    std::memset(&mousePrev_, 0, sizeof(mousePrev_));
}

// ===== Keyboard =====
bool Input::IsKeyPressed(std::uint8_t dik) const noexcept {
    assert(dik < 256);
    return (nowKey_[dik] & KEY_PRESSED_MASK) != 0;
}

bool Input::IsKeyTriggered(std::uint8_t dik) const noexcept {
    assert(dik < 256);
    return !(prevKey_[dik] & KEY_PRESSED_MASK) && (nowKey_[dik] & KEY_PRESSED_MASK);
}

bool Input::IsKeyReleased(std::uint8_t dik) const noexcept {
    assert(dik < 256);
    return (prevKey_[dik] & KEY_PRESSED_MASK) && !(nowKey_[dik] & KEY_PRESSED_MASK);
}

// ===== Mouse Buttons =====
bool Input::IsMousePressed(int button) const noexcept {
    if (button < 0 || button >= MOUSE_BUTTONS) return false;
    return (mouseNow_.rgbButtons[button] & KEY_PRESSED_MASK) != 0;
}

bool Input::IsMouseTriggered(int button) const noexcept {
    if (button < 0 || button >= MOUSE_BUTTONS) return false;
    return !(mousePrev_.rgbButtons[button] & KEY_PRESSED_MASK) &&
        (mouseNow_.rgbButtons[button] & KEY_PRESSED_MASK);
}

bool Input::IsMouseReleased(int button) const noexcept {
    if (button < 0 || button >= MOUSE_BUTTONS) return false;
    return (mousePrev_.rgbButtons[button] & KEY_PRESSED_MASK) &&
        !(mouseNow_.rgbButtons[button] & KEY_PRESSED_MASK);
}

// ===== Acquire helpers =====
bool Input::TryAcquireKeyboard_() noexcept {
    if (!keyboard_) return false;
    HRESULT hr = keyboard_->Acquire();
    if (SUCCEEDED(hr)) return true;
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        hr = keyboard_->Acquire();
        return SUCCEEDED(hr);
    }
    return false;
}

bool Input::TryAcquireMouse_() noexcept {
    if (!mouse_) return false;
    HRESULT hr = mouse_->Acquire();
    if (SUCCEEDED(hr)) return true;
    if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
        hr = mouse_->Acquire();
        return SUCCEEDED(hr);
    }
    return false;
}
