#pragma once
#include <windows.h>
#include <string>

class WinApp {
public:
	/// <summary>
	/// デストラクタ
	/// </summary>
	~WinApp();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="clientWidth">クライアント領域の横幅</param>
	/// <param name="clientHeight">クライアント領域の高さ</param>
	/// <param name="windowTitle">ウィンドウタイトル</param>
	void Initialize(LONG clientWidth, LONG clientHeight, const std::wstring &windowTitle);

	/// <summary>
	/// メッセージポンプを1回分実行する
	/// </summary>
	bool ProcessMessage();

	/// <summary>
	/// ウィンドウハンドルを取得する
	/// </summary>
	HWND GetHwnd() const { return hwnd_; }

	/// <summary>
	/// インスタンスハンドルを取得
	/// </summary>
	/// <returns>インスタンスハンドル</returns>
	HINSTANCE GetHInstance() const { return wc_.hInstance; }

private:
	/// <summary>
	/// ウィンドウプロシージャ
	/// </summary>
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	HWND hwnd_;
	WNDCLASS wc_{};
};
