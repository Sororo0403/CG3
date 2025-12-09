#include "FileLogger.h"
#include <filesystem>
#include <iostream>

static std::string NowTimeString() {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto itt = system_clock::to_time_t(now);

  std::tm tm{};
  localtime_s(&tm, &itt);

  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return buf;
}

FileLogger::~FileLogger() {
  if (file_.is_open()) {
    file_.close();
  }
}

void FileLogger::Initialize() {
  namespace fs = std::filesystem;

  fs::path exe = fs::current_path();

  // Visual Studio の $(Configuration) をそのまま使う
#ifndef CONFIG_NAME
#define CONFIG_NAME "Unknown"
#endif

  std::string config = CONFIG_NAME; // "Debug" / "Release" / "Development" など

  // 日付入りログ名
  auto t = NowTimeString();
  std::string day = t.substr(0, 10);

  fs::path dir = exe / "../Generated/Outputs" / config / "Logs";
  fs::create_directories(dir);

  filePath_ = (dir / (day + ".log")).string();
  file_.open(filePath_, std::ios::out | std::ios::app);
}

void FileLogger::Write(LogLevel level, const std::string &message,
                       const char *file, int line) {
  if (!file_.is_open()) {
    return;
  }

  std::string time = NowTimeString();

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

