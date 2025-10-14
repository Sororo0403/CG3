#pragma once
#include <cstdint>
#include <functional>
#include <windows.h>

class WinApp {
public:
	static constexpr uint32_t kClientWidth = 1280;  // クライアント領域の幅
	static constexpr uint32_t kClientHeight = 720;  // クライアント領域の高さ
public:
	/// <summary>
	/// ウィンドウを生成・表示し、WinApp を初期化する
	/// </summary>
	void Initialize();

	/// <summary>
	/// メッセージポンプを1回分実行する
	/// </summary>
	bool ProcessMessage();

	/// <summary>
	/// サイズ変更時に呼び出されるコールバックを登録する
	/// </summary>
	void SetOnResize(
		std::function<void(uint32_t width, uint32_t height, UINT state)> cb) {
		onResize_ = std::move(cb);
	}

	/// <summary>
	/// ウィンドウハンドルを取得する
	/// </summary>
	HWND GetHwnd() const { return hwnd_; }

	/// <summary>
	/// インスタンスハンドルを取得する
	/// </summary>
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

	/// <summary>
	/// ウィンドウプロシージャ
	/// </summary>
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

	/// <summary>
	/// 解放処理
	/// </summary>
	void Finalize();

private:
	/// <summary>
	/// HWND にぶら下げられた自分自身のポインタを取得
	/// </summary>
	static WinApp *FromHwnd(HWND hwnd) {
		return reinterpret_cast<WinApp *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

private:
	HWND hwnd_ = nullptr;
	WNDCLASS wc_{};
	std::function<void(uint32_t, uint32_t, UINT)> onResize_;
};
