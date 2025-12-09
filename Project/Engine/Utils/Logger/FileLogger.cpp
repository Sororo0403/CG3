#include "FileLogger.h"
#include "Time/TimeUtils.h"
#include <filesystem>
#include <iostream>

FileLogger::~FileLogger() {
  if (file_.is_open()) {
    file_.close();
  }
}

void FileLogger::Initialize() {
  std::filesystem::path exe = std::filesystem::current_path();

#ifndef CONFIG_NAME
#define CONFIG_NAME "Unknown"
#endif

  std::string config = CONFIG_NAME;

  // 日付入りログ名
  auto t = TimeUtils::NowTimeString();
  std::string day = t.substr(0, 10);

  std::filesystem::path dir = exe / "../Generated/Outputs" / config / "Logs";
  std::filesystem::create_directories(dir);

  filePath_ = (dir / (day + ".log")).string();
  file_.open(filePath_, std::ios::out | std::ios::app);
}

void FileLogger::Write(LogLevel level, const std::string &message,
                       const char *file, int line) {
  if (!file_.is_open()) {
    return;
  }

  std::string time = TimeUtils::NowTimeString();

  const char *levelStr = "";
  switch (level) {
  case LogLevel::INFO:
    levelStr = "INFO";
    break;
  case LogLevel::WARNING:
    levelStr = "WARNING";
    break;
  case LogLevel::ERR:
    levelStr = "ERROR";
    break;
  case LogLevel::DEBUG:
    levelStr = "DEBUG";
    break;
  }

  // 書式整形して出力
  file_ << "[" + time + "][" + levelStr + "] " + message + " (" + file + ":" +
               std::to_string(line) + ")"
        << std::endl;
}
