#pragma once

#include <memory>
#include <vector>

#include "ILogger.h"
#include "LogLevel.h"

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
  LoggerManager(const LoggerManager &) = delete;
  LoggerManager &operator=(const LoggerManager &) = delete;

private:
  LogLevel minLevel_ = LogLevel::DEBUG;

  std::vector<std::unique_ptr<ILogger>> loggers_;
};
