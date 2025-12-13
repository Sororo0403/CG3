#pragma once

#include <string>

#include "LogLevel.h"

class ILogger {
public:
  /// <summary>
  /// デストラクタ
  /// </summary>
  virtual ~ILogger() = default;

  /// <summary>
  /// ログ情報を書き込み
  /// </summary>
  /// <param name="level">ログレベル</param>
  /// <param name="message">メッセージ</param>
  /// <param name="file">ログ発生元のファイル名</param>
  /// <param name="line">ログ発生元の行番号</param>
  virtual void Write(LogLevel level, const std::string &message,
                     const char *file, int line) = 0;
};
