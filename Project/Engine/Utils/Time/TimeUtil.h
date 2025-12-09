#pragma once
#include <chrono>
#include <string>

namespace TimeUtil {

/// <summary>
/// "YYYY-MM-DD_HH-MM-SS" の形式で現在時刻を返す
/// </summary>
/// <returns>時刻文字列</returns>
inline std::string NowTimeString() {
  auto now = std::chrono::system_clock::now();
  auto itt = std::chrono::system_clock::to_time_t(now);

  std::tm tm{};
  localtime_s(&tm, &itt);

  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", &tm);
  return buf;
}

} // namespace TimeUtil