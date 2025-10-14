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
	/// シングルトンインスタンスを取得
	/// </summary>
	/// <returns>LoggerManagerの参照</returns>
	static LoggerManager &GetInstance() noexcept;

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

// 簡易マクロ群
#define LOG_TRACE(msg) LoggerManager::GetInstance().Log(LogLevel::TRACE, (msg))
#define LOG_DEBUG(msg) LoggerManager::GetInstance().Log(LogLevel::DEBUG, (msg))
#define LOG_INFO(msg)  LoggerManager::GetInstance().Log(LogLevel::INFO,  (msg))
#define LOG_WARN(msg)  LoggerManager::GetInstance().Log(LogLevel::WARN,  (msg))
#define LOG_ERROR(msg) LoggerManager::GetInstance().Log(LogLevel::ERR, (msg))
#define LOG_FATAL(msg) LoggerManager::GetInstance().Log(LogLevel::FATAL, (msg))