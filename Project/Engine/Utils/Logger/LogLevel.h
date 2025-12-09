#pragma once
#include <string_view>

// ログレベル
enum class LogLevel { DEBUG, INFO, WARNING, ERR };

/// <summary>
/// LogLevel を文字列に変換します。
/// </summary>
/// <returns>レベル名</returns>
constexpr std::string_view ToString(LogLevel level) {
  if (level == LogLevel::INFO) {
    return "INFO";
  }
  if (level == LogLevel::WARNING) {
    return "WARNING";
  }
  if (level == LogLevel::ERR) {
    return "ERROR";
  }
  if (level == LogLevel::DEBUG) {
    return "DEBUG";
  }
  return "UNKNOWN";
}
