#pragma once
#include "ILogger.h"
#include <fstream>
#include <string>

class FileLogger : public ILogger {
public:
  /// <summary>
  /// デストラクタ
  /// </summary>
  ~FileLogger() override;

  /// <summary>
  /// 初期化処理
  /// </summary>
  void Initialize() override;

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
