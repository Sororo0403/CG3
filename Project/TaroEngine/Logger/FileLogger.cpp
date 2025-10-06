#include "FileLogger.h"
#include <format>

FileLogger::FileLogger(const std::string &filePath)
    : filePath_(filePath), ofs_(filePath, std::ios::app)
{
    if (ofs_.is_open()) {
        ofs_ << "===== Log session started at "
            << LogTimeUtil::GetCurrentTimeString() << " =====\n";
        ofs_.flush();
    }
}

FileLogger::~FileLogger() {
    if (ofs_.is_open()) {
        ofs_ << "===== Log session ended at "
            << LogTimeUtil::GetCurrentTimeString() << " =====\n";
        ofs_.flush();
        ofs_.close();
    }
}

void FileLogger::Log(LogLevel level, const std::string &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ofs_.is_open()) return;

    std::string timeStr = LogTimeUtil::GetCurrentTimeString();
    const char *levelStr = LogLevelUtil::ToString(level);

    std::string formatted = std::format("[{}] [{:<5}] {}\n", timeStr, levelStr, msg);

    ofs_ << formatted;
    ofs_.flush(); 
}

bool FileLogger::IsOpen() const {
    return ofs_.is_open();
}
