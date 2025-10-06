#pragma once

#include <string>

/// <summary>
/// ロガーの共通インターフェイス。
/// </summary>
class ILogger {
public:
	/// <summary>
	/// デストラクタ。
	/// </summary>
	virtual ~ILogger() = default;

	/// <summary>
	/// メッセージを出力
	/// </summary>
	/// <param name="msg">出力するメッセージ文字列。</param>
	virtual void Log(const std::string &msg) = 0;
};