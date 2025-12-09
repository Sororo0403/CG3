#pragma once
#include "ILogger.h"
#include "LogLevel.h"
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

#define LOG_INFO(msg)                                                          \
  LoggerManager::GetInstance()->Write(LogLevel::INFO, msg, __FILE__, __LINE__)
#define LOG_WARN(msg)                                                          \
  LoggerManager::GetInstance()->Write(LogLevel::WARNING, msg, __FILE__,        \
                                      __LINE__)
#define LOG_ERROR(msg)                                                         \
  LoggerManager::GetInstance()->Write(LogLevel::ERR, msg, __FILE__, __LINE__)
#define LOG_DEBUG(msg)                                                         \
  LoggerManager::GetInstance()->Write(LogLevel::DEBUG, msg, __FILE__, __LINE__)