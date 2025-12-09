#pragma once
#include <chrono>
#include <string>

namespace TimeUtils {

/// <summary>
/// 現在のローカル日時を "YYYY-MM-DD HH:MM:SS" 形式の文字列として返します
/// </summary>
/// <returns>フォーマット済みの現在時刻文字列</returns>
inline std::string NowTimeString() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
  localtime_s(&tm, &itt);

  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
  return buf;
}

} // namespace TimeUtils