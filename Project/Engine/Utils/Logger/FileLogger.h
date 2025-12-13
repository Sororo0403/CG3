#pragma once

#include <fstream>
#include <string>

#include "ILogger.h"

class FileLogger : public ILogger {
public:
  /// <summary>
  /// コンストラクタ
  /// </summary>
  FileLogger();

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~FileLogger() override;

  /// <summary>
  /// ログ情報を書き込み
  /// </summary>
  /// <param name="level">ログレベル</param>
  /// <param name="message">メッセージ</param>
  /// <param name="file">ログ発生元のファイル名</param>
  /// <param name="line">ログ発生元の行番号</param>
  void Write(LogLevel level, const std::string &message, const char *file,
             int line) override;

private:
  std::ofstream file_;
  std::string filePath_;
};
