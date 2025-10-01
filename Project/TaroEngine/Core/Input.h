#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include "WinApp.h"
#include <dinput.h>
#include <wrl.h>

/// <summary>
/// DirectInput を使ったキーボード入力管理クラス。<br/>
/// 毎フレームのキー状態を保持し、押下・離し・トリガー判定を提供する。
/// </summary>
class Input {
private:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    /// <summary>
    /// DirectInput の「キー押下中」を示すビットマスク値。
    /// </summary>
    static constexpr BYTE KEY_PRESSED_MASK = 0x80;

public:
    // ===============================
    // ライフサイクル
    // ===============================

    /// <summary>
    /// DirectInput を初期化し、キーボードデバイスを生成する。<br/>
    /// WinApp から HINSTANCE / HWND を取得して利用する。
    /// </summary>
    /// <param name="winApp">アプリの WinApp。</param>
    void Initialize(WinApp *winApp);

    /// <summary>
    /// 毎フレーム呼び出し、前フレームと現在フレームのキー状態を更新する。
    /// </summary>
    void Update();

    // ===============================
    // 入力判定
    // ===============================

    /// <summary>
    /// 指定したキーが押され続けているかを返す。
    /// </summary>
    /// <param name="dik">DIK_ 定数。</param>
    bool IsKeyDown(BYTE dik) const { return (now_[dik] & KEY_PRESSED_MASK) != 0; }

    /// <summary>
    /// 指定したキーが「離された瞬間」かを返す。
    /// </summary>
    /// <param name="dik">DIK_ 定数。</param>
    bool IsKeyReleased(BYTE dik) const {
        return (prev_[dik] & KEY_PRESSED_MASK) && !(now_[dik] & KEY_PRESSED_MASK);
    }

    /// <summary>
    /// 指定したキーが「押された瞬間」かを返す。
    /// </summary>
    /// <param name="dik">DIK_ 定数。</param>
    bool IsKeyPressed(BYTE dik) const {
        return !(prev_[dik] & KEY_PRESSED_MASK) && (now_[dik] & KEY_PRESSED_MASK);
    }

private:
    // ===============================
    // メンバ変数
    // ===============================

    WinApp *winApp_ = nullptr;             // WinApp への参照
    ComPtr<IDirectInput8> di_;             // DirectInput 本体
    ComPtr<IDirectInputDevice8> keyboard_; // キーボードデバイス
    BYTE now_[256] = {};                   // 今フレームのキー状態
    BYTE prev_[256] = {};                  // 前フレームのキー状態
};
