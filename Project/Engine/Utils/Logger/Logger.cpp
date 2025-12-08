#include "Logger.h"
#include <chrono>
#include <iomanip>
#include <iostream>

Logger *Logger::GetInstance() {
  static Logger instance;
  return &instance;
}

void Logger::Initialize(const std::string &filePath) {
  std::lock_guard<std::mutex> lock(mtx_);
  file_.open(filePath, std::ios::out | std::ios::app);
}

Logger::~Logger() {
  if (file_.is_open()) {
    file_.close();
  }
}

static std::string NowTimeString() {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto itt = system_clock::to_time_t(now);

  std::tm tm;
  localtime_s(&tm, &itt);

  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return buf;
}

static const char *LevelToString(LogLevel level) {
  switch (level) {
  case LogLevel::INFO:
    return "INFO";
  case LogLevel::WARNING:
    return "WARNING";
  case LogLevel::ERROR:
    return "ERROR";
  case LogLevel::DEBUG:
    return "DEBUG";
  }
  return "UNKNOWN";
}

void Logger::Write(LogLevel level, const std::string &message, const char *file,
                   int line) {
  std::lock_guard<std::mutex> lock(mtx_);

  std::string time = NowTimeString();
  std::string logLine = "[" + time + "][" + LevelToString(level) + "] " +
                        message + " (" + file + ":" + std::to_string(line) +
                        ")";

  if (file_.is_open()) {
    file_ << logLine << std::endl;
  }

  std::cout << logLine << std::endl;
}

void Logger::Info(const std::string &msg, const char *file, int line) {
  Write(LogLevel::INFO, msg, file, line);
}

void Logger::Warning(const std::string &msg, const char *file, int line) {
  Write(LogLevel::WARNING, msg, file, line);
}

void Logger::Error(const std::string &msg, const char *file, int line) {
  Write(LogLevel::ERROR, msg, file, line);
}

void Logger::Debug(const std::string &msg, const char *file, int line) {
#if _DEBUG
  Write(LogLevel::DEBUG, msg, file, line);
#endif
}
