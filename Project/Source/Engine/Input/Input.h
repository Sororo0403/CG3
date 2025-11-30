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
    /// <returns>成功なら true</returns>
    bool Initialize(HINSTANCE hInstance, HWND hwnd) noexcept;

    /// <summary>
    /// 毎フレーム呼び出してキーボード/マウスの状態を更新
    /// </summary>
    void Update() noexcept;

    /// <summary>
    /// 解放処理
    /// </summary>
    void Finalize() noexcept;

    /// <summary>
    /// 現在と前フレームのキー/マウス状態をすべて初期化
    /// </summary>
    void ResetStates() noexcept;

    // ===== Keyboard =====
    /// <summary>指定キーが押されているか</summary>
    bool IsKeyPressed(std::uint8_t dik) const noexcept;
    /// <summary>指定キーが今フレームで押されたか</summary>
    bool IsKeyTriggered(std::uint8_t dik) const noexcept;
    /// <summary>指定キーが今フレームで離されたか</summary>
    bool IsKeyReleased(std::uint8_t dik) const noexcept;

    // ===== Mouse Buttons (0..7) =====
    /// <summary>ボタンが押されているか（0:左,1:右,2:中, 3..7:拡張）</summary>
    bool IsMousePressed(int button) const noexcept;
    /// <summary>ボタンが今フレームで押されたか</summary>
    bool IsMouseTriggered(int button) const noexcept;
    /// <summary>ボタンが今フレームで離されたか</summary>
    bool IsMouseReleased(int button) const noexcept;

    // ===== Mouse Movement =====
    /// <summary>このフレームの相対移動量（Δ）</summary>
    LONG GetMouseDeltaX() const noexcept { return mouseNow_.lX; }
    LONG GetMouseDeltaY() const noexcept { return mouseNow_.lY; }
    /// <summary>このフレームのホイール量（上+ / 下-）</summary>
    LONG GetMouseWheelDelta() const noexcept { return mouseNow_.lZ; }

private:
    /// <summary>キーボードの Acquire 試行</summary>
    bool TryAcquireKeyboard_() noexcept;
    /// <summary>マウスの Acquire 試行</summary>
    bool TryAcquireMouse_() noexcept;

private:
    // 押されているかを示すビットマスク（
    static constexpr BYTE KEY_PRESSED_MASK = 0x80;
    static constexpr int  MOUSE_BUTTONS = 8;

    // DirectInput 共通
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

    // Keyboard
    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE nowKey_[256] = {};
    BYTE prevKey_[256] = {};

    // Mouse
    Microsoft::WRL::ComPtr<IDirectInputDevice8> mouse_;
    DIMOUSESTATE2 mouseNow_ = {};
    DIMOUSESTATE2 mousePrev_ = {};
};
