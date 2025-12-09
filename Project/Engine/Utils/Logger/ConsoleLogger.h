#pragma once
#include "ILogger.h"

class ConsoleLogger : public ILogger {
public:
  /// <summary>
  /// ログをコンソールへ出力します
  /// </summary>
  /// <param name="level">ログレベル</param>
  /// <param name="message">メッセージ</param>
  /// <param name="file">発生元ファイル名</param>
  /// <param name="line">発生元行番号</param>
  void Write(LogLevel level, const std::string &message, const char *file,
             int line) override;
};