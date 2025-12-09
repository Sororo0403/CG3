#include "LoggerManager.h"

LoggerManager *LoggerManager::GetInstance() {
  static LoggerManager instance;
  return &instance;
}

void LoggerManager::AddLogger(std::unique_ptr<ILogger> logger) {
  loggers_.push_back(std::move(logger));
}

void LoggerManager::Write(LogLevel level, const std::string &message,
                          const char *file, int line) {
  if (static_cast<int>(level) < static_cast<int>(minLevel_)) {
    return;
  }

  for (auto &logger : loggers_) {
    logger->Write(level, message, file, line);
  }
}
