#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <wrl.h>
#include <cstdint>

class Input {
public:
    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="hInstance">アプリケーションのインスタンスハンドル</param>
    /// <param name="hwnd">アプリケーションのウィンドウハンドル</param>
    /// <returns>初期化に成功した場合は true、失敗した場合は false を返します</returns>
    bool Initialize(HINSTANCE hInstance, HWND hwnd) noexcept;

    /// <summary>
    /// 毎フレーム呼び出してキーボードの状態を更新します。
    /// </summary>
    void Update() noexcept;

    /// <summary>
    /// 解放処理
    /// </summary>
    void Finalize() noexcept;

    /// <summary>
    /// 現在と前フレームのキー状態をすべて初期化
    /// </summary>
    void ResetStates() noexcept;

    /// <summary>
    /// 指定されたキーが押されているかを返す
    /// </summary>
    /// <param name="dik">DirectInput のキーコード (DIK_*)</param>
    /// <returns>押されている場合は true</returns>
    bool IsKeyDown(std::uint8_t dik) const noexcept;

    /// <summary>
    /// 指定されたキーが「今フレームで新たに押された」かを返す
    /// </summary>
    /// <param name="dik">DirectInput のキーコード (DIK_*)</param>
    /// <returns>押された瞬間なら true</returns>
    bool IsKeyPressed(std::uint8_t dik) const noexcept;

    /// <summary>
    /// 指定されたキーが「今フレームで離された」かを返す
    /// </summary>
    /// <param name="dik">DirectInput のキーコード (DIK_*)</param>
    /// <returns>離された瞬間なら true</returns>
    bool IsKeyReleased(std::uint8_t dik) const noexcept;

private:
    /// <summary>
    /// 入力デバイスを取得状態にする
    /// </summary>
    /// <returns>取得に成功した場合は true</returns>
    bool TryAcquire() noexcept;

private:
	// 押されているかを示すビットマスク
    static constexpr BYTE KEY_PRESSED_MASK = 0x80;

	// DirectInput 関連
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE now_[256] = {};
    BYTE prev_[256] = {};
};