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

	hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
	if (FAILED(hr)) return false;

	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
	if (FAILED(hr)) return false;

	hr = keyboard_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	if (FAILED(hr)) return false;

	ResetStates();
	TryAcquire();
	return true;
}

void Input::Update() noexcept {
	if (!keyboard_) return;

	std::memcpy(prev_, now_, sizeof(now_));
	if (!TryAcquire()) {
		std::memset(now_, 0, sizeof(now_));
		return;
	}

	HRESULT hr = keyboard_->GetDeviceState(sizeof(now_), now_);
	if (FAILED(hr)) {
		std::memset(now_, 0, sizeof(now_));
		TryAcquire();
	}
}

void Input::Finalize() noexcept {
	if (keyboard_) keyboard_->Unacquire();
	keyboard_.Reset();
	directInput_.Reset();
	ResetStates();
}

void Input::ResetStates() noexcept {
	std::memset(now_, 0, sizeof(now_));
	std::memset(prev_, 0, sizeof(prev_));
}

bool Input::IsKeyDown(std::uint8_t dik) const noexcept {
	assert(dik < 256);
	return (now_[dik] & KEY_PRESSED_MASK) != 0;
}

bool Input::IsKeyPressed(std::uint8_t dik) const noexcept {
	assert(dik < 256);
	return !(prev_[dik] & KEY_PRESSED_MASK) && (now_[dik] & KEY_PRESSED_MASK);
}

bool Input::IsKeyReleased(std::uint8_t dik) const noexcept {
	assert(dik < 256);
	return (prev_[dik] & KEY_PRESSED_MASK) && !(now_[dik] & KEY_PRESSED_MASK);
}

bool Input::TryAcquire() noexcept {
	if (!keyboard_) return false;
	HRESULT hr = keyboard_->Acquire();
	if (SUCCEEDED(hr)) return true;
	if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
		hr = keyboard_->Acquire();
		return SUCCEEDED(hr);
	}
	return false;
}