#pragma once

#include <chrono>
#include <string>
#include "LogLevel.h"

/// <summary>
/// ログ1件分の情報
/// </summary>
struct LogMessage {
	std::chrono::system_clock::time_point time;
	LogLevel level;
	std::string category;
	std::string text;
	uint64_t threadId = 0;
	const char *file = nullptr;
	int line = 0;
};
