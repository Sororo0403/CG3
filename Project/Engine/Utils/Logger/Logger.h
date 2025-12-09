#pragma once
#include "LoggerManager.h"

#define LOG(level, msg)                                                        \
  LoggerManager::GetInstance()->Write(level, msg, __FILE__, __LINE__)

#define LOG_INFO(msg) LOG(LogLevel::INFO, msg)
#define LOG_WARN(msg) LOG(LogLevel::WARNING, msg)
#define LOG_ERROR(msg) LOG(LogLevel::ERR, msg)
#define LOG_DEBUG(msg) LOG(LogLevel::DEBUG, msg)