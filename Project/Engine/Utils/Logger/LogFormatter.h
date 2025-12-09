#pragma once
#include "LogLevel.h"
#include <string>

namespace LogFormatter {

/// <summary>
/// 共通のログフォーマット文字列を組み立てる
/// </summary>
/// <returns>書式済みログ文字列</returns>
inline std::string Format(const std::string &time, LogLevel level,
                          const std::string &message, const char *file,
                          int line) {
  return "[" + time + "][" + std::string(ToString(level)) + "] " + message +
         " (" + file + ":" + std::to_string(line) + ")";
}

} // namespace LogFormatter
