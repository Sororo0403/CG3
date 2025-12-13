#pragma once

#include <string>

#include "LogLevel.h"

namespace LogFormatter {

/// <summary>
/// フルパス文字列から末尾のファイル名部分のみを取り出す
/// </summary>
inline std::string_view ExtractFileName(const char *path) {
  std::string_view s(path);
  size_t pos = s.find_last_of("/\\");
  return (pos == std::string_view::npos) ? s : s.substr(pos + 1);
}

/// <summary>
/// 共通のログフォーマット文字列を組み立てる
/// </summary>
/// <returns>書式済みログ文字列</returns>
inline std::string Format(const std::string &time, LogLevel level,
                          const std::string &message, const char *file,
                          int line) {

  std::string_view filename = ExtractFileName(file);

  return "[" + time + "][" + std::string(ToString(level)) + "] " + message +
         " (" + std::string(filename) + ":" + std::to_string(line) + ")";
}

} // namespace LogFormatter
