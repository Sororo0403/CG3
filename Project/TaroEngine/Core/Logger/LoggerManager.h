#pragma once
#include <vector>
#include <memory>
#include <mutex>
#include <string_view>
#include "ILogger.h"
#include "LogLevel.h"

class LoggerManager final : public ILogger {
public:
	/// <summary>
	/// デストラクタ
	/// </summary>
	~LoggerManager() override;

	/// <summary>
	/// ログ出力
	/// </summary>
	/// <param name="logLevel">ログレベル</param>
	/// <param name="message">メッセージ</param>
	void Log(LogLevel logLevel, std::string_view message) override;

	/// <summary>
	/// ログ出力
	/// </summary>
	/// <param name="logLevel">ログレベル</param>
	/// <param name="message">メッセージ</param>
	void Log(LogLevel logLevel, std::wstring_view message) override;

	/// <summary>
	/// ロガーを追加
	/// </summary>
	/// <param name="logger">追加するロガー</param>
	void AddLogger(const std::shared_ptr<ILogger> &logger);

	/// <summary>
	/// すべてのロガーを削除
	/// </summary>
	void ClearLoggers();

private:
	mutable std::mutex mutex_;
	std::vector<std::shared_ptr<ILogger>> loggers_;
};