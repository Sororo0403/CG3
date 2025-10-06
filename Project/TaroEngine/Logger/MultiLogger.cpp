#include "MultiLogger.h"

void MultiLogger::AddLogger(const std::shared_ptr<ILogger> &logger) {
    if (!logger) return;
    std::lock_guard<std::mutex> lock(mutex_);
    loggers_.push_back(logger);
}

void MultiLogger::Log(LogLevel level, const std::string &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &logger : loggers_) {
        if (logger) {
            logger->Log(level, msg);
        }
    }
}
