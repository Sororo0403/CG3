#include "FileLogger.h"
#include "LogFormatter.h"
#include "Time/TimeUtil.h"
#include <filesystem>
#include <iostream>

FileLogger::FileLogger() {
  namespace fs = std::filesystem;

  fs::path exe = fs::current_path();

  // VS プロジェクトから渡した構成名
  const std::string config = CONFIG_NAME;

  // 日付入りログ名
  const std::string day = TimeUtil::NowTimeString();

  // 出力先ディレクトリ
  fs::path dir = exe / "../Generated/Outputs" / config / "Logs";

  // ディレクトリを必ず作成
  fs::create_directories(dir);

  // ログファイル名
  filePath_ = (dir / (day + ".log")).string();

  // 追記モードで開く
  file_.open(filePath_, std::ios::out | std::ios::app);

  if (!file_.is_open()) {
    std::cerr << "FileLogger: Failed to open log file: " << filePath_
              << std::endl;
  }
}

FileLogger::~FileLogger() {
  if (file_.is_open()) {
    file_.close();
  }
}

void FileLogger::Write(LogLevel level, const std::string &message,
                       const char *file, int line) {
  if (!file_.is_open())
    return;

  std::string time = TimeUtil::NowTimeString();
  std::string formatted =
      LogFormatter::Format(time, level, message, file, line);

  file_ << formatted << std::endl;
}
