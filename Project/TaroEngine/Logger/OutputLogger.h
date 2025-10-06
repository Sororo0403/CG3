#pragma once
#include "ILogger.h"
#include <iostream>
#include <mutex>

/// <summary>
/// 標準出力にログメッセージを出力するロガー。
/// </summary>
class OutputLogger : public ILogger {
public:
	/// <summary>
	/// デストラクタ。
	/// </summary>
	~OutputLogger() override = default;

	/// <summary>
	/// ログレベルを指定してメッセージを出力します。
	/// </summary>
	/// <param name="level">ログレベル。</param>
	/// <param name="msg">出力するメッセージ文字列。</param>
	void Log(LogLevel level, const std::string &msg) override;

private:
	std::mutex mutex_; ///< マルチスレッド出力防止用。
};
