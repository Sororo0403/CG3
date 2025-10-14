#include "LoggerManager.h"

LoggerManager &LoggerManager::GetInstance() noexcept {
    static LoggerManager instance;
    return instance;
}

LoggerManager::~LoggerManager() = default;

void LoggerManager::Log(LogLevel logLevel, std::string_view message) {
    // ロガー一覧のスナップショットを作ってロック保持時間を最小化
    std::vector<std::shared_ptr<ILogger>> targets;
    {
        std::scoped_lock lock(mutex_);
        targets = loggers_;
    }

    for (const auto &logger : targets) {
        if (logger) {
            logger->Log(logLevel, message);
        }
    }
}

void LoggerManager::AddLogger(const std::shared_ptr<ILogger> &logger) {
    if (!logger) return;
    std::scoped_lock lock(mutex_);
    loggers_.push_back(logger);
}

void LoggerManager::ClearLoggers() {
    std::scoped_lock lock(mutex_);
    loggers_.clear();
}
