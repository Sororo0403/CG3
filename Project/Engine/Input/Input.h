#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <Windows.h>
#include <dinput.h>
#include <wrl.h>
#include <cstdint>

class Input {
  public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~Input();

    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="hInstance">アプリケーションのインスタンスハンドル</param>
    /// <param name="hwnd">アプリケーションのウィンドウハンドル</param>
    void Initialize(HINSTANCE hInstance, HWND hwnd);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update();

    /// <summary>
    /// キー状態を初期化
    /// </summary>
    void ResetStates();

    // Getter
    bool IsKeyPressed(std::uint8_t dik) const;
    bool IsKeyTriggered(std::uint8_t dik) const;
    bool IsKeyReleased(std::uint8_t dik) const;

  private:
    /// <summary>
    /// キーボードの Acquire 試行
    /// </summary>
    bool TryAcquireKeyboard_();

  private:
    static constexpr BYTE KEY_PRESSED_MASK = 0x80;

  private:
    Microsoft::WRL::ComPtr<IDirectInput8> directInput_;

    Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_;
    BYTE nowKey_[256] = {};
    BYTE prevKey_[256] = {};
};
