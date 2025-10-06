#include "OutputLogger.h"
#include "LogTimeUtil.h"
#include "LogLevelUtil.h"
#include <windows.h>

void OutputLogger::Log(LogLevel level, const std::string &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string timeStr = LogTimeUtil::GetCurrentTimeString();
    const char *levelStr = LogLevelUtil::ToString(level);

    std::string formatted = std::format("[{}] [{:<5}] {}\n", timeStr, levelStr, msg);
    OutputDebugStringA(formatted.c_str());
}