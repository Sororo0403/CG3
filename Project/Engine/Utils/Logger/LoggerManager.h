#pragma once
#include "ILogger.h"
#include "LogLevel.h"
#include <format>
#include <memory>
#include <vector>

class LoggerManager {
public:
  /// <summary>
  /// 唯一のインスタンス取得
  /// </summary>
  /// <returns>唯一のインスタンス</returns>
  static LoggerManager *GetInstance();

  /// <summary>
  /// ロガーを追加
  /// </summary>
  /// <param name="logger">追加するロガーのスマートポインタ</param>
  void AddLogger(std::unique_ptr<ILogger> logger);

  /// <summary>
  /// 全ロガーに書き込み
  /// </summary>
  /// <param name="level">ログレベル</param>
  /// <param name="message">メッセージ</param>
  /// <param name="file">ログ発生元のファイル名</param>
  /// <param name="line">ログ発生元の行番号</param>
  void Write(LogLevel level, const std::string &message, const char *file,
             int line);

  /// <summary>
  /// 出力する最小ログレベル
  /// </summary>
  void SetMinLevel(LogLevel level) { minLevel_ = level; }

private:
  // シングルトン
  LoggerManager() = default;
  ~LoggerManager() = default;

private:
  LogLevel minLevel_ = LogLevel::DEBUG;

  std::vector<std::unique_ptr<ILogger>> loggers_;
};

#define LOG_INTERNAL(level, fmt, ...)                                          \
  LoggerManager::GetInstance()->Write(level, std::format(fmt, ##__VA_ARGS__),  \
                                      __FILE__, __LINE__)

#define LOG_INFO(fmt, ...) LOG_INTERNAL(LogLevel::INFO, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_INTERNAL(LogLevel::WARNING, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_INTERNAL(LogLevel::ERR, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_INTERNAL(LogLevel::DEBUG, fmt, ##__VA_ARGS__)