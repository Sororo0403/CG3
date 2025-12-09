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
  for (auto &l : loggers_) {
    l->Write(level, message, file, line);
  }
}
