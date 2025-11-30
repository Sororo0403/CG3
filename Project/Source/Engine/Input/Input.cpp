#include "Input.h"
#include <cassert>
#include <cstring>

Input::~Input() {
	if (keyboard_) {
		keyboard_->Unacquire();
	}
	keyboard_.Reset();
	directInput_.Reset();

	ResetStates();
}

void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
	HRESULT hr = DirectInput8Create(
		hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void **>(directInput_.GetAddressOf()),
		nullptr);

	assert(SUCCEEDED(hr));

	// キーボード
	hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
	assert(SUCCEEDED(hr));

	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(hr));

	hr = keyboard_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(hr));

	ResetStates();

	// 最初に Acquire
	TryAcquireKeyboard_();
}

void Input::Update() {
	// 前フレーム → 今フレーム
	std::memcpy(prevKey_, nowKey_, sizeof(nowKey_));

	// 現在のキーを取得
	if (!TryAcquireKeyboard_()) {
		std::memset(nowKey_, 0, sizeof(nowKey_));
		return;
	}

	HRESULT hr = keyboard_->GetDeviceState(static_cast<DWORD>(sizeof(nowKey_)), nowKey_);
	if (FAILED(hr)) {
		std::memset(nowKey_, 0, sizeof(nowKey_));
		TryAcquireKeyboard_();
	}
}

void Input::ResetStates() {
	std::memset(nowKey_, 0, sizeof(nowKey_));
	std::memset(prevKey_, 0, sizeof(prevKey_));
}

bool Input::IsKeyPressed(std::uint8_t dik) const {
	assert(dik < 256);
	return (nowKey_[dik] & KEY_PRESSED_MASK) != 0;
}

bool Input::IsKeyTriggered(std::uint8_t dik) const {
	assert(dik < 256);
	return !(prevKey_[dik] & KEY_PRESSED_MASK) &&
		(nowKey_[dik] & KEY_PRESSED_MASK);
}

bool Input::IsKeyReleased(std::uint8_t dik) const {
	assert(dik < 256);
	return (prevKey_[dik] & KEY_PRESSED_MASK) &&
		!(nowKey_[dik] & KEY_PRESSED_MASK);
}

bool Input::TryAcquireKeyboard_() {
	if (!keyboard_) return false;

	HRESULT hr = keyboard_->Acquire();
	if (SUCCEEDED(hr)) return true;

	if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
		hr = keyboard_->Acquire();
		return SUCCEEDED(hr);
	}
	return false;
}
