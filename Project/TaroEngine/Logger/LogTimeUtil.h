#pragma once
#include <chrono>
#include <string>
#include <iomanip>
#include <sstream>
#include <ctime>

/// <summary>
/// ログ用の時刻ユーティリティ関数群。
/// </summary>
namespace LogTimeUtil {

    /// <summary>
    /// 現在時刻を "HH:MM:SS" 形式で文字列に変換します。
    /// </summary>
    /// <returns>フォーマット済みの現在時刻文字列。</returns>
    inline std::string GetCurrentTimeString() {
        using namespace std::chrono;
        std::time_t t = system_clock::to_time_t(system_clock::now());
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }

    /// <summary>
    /// 任意の時刻を "HH:MM:SS" 形式で文字列に変換します。
    /// </summary>
    /// <param name="tp">変換対象の時刻。</param>
    /// <returns>フォーマット済みの時刻文字列。</returns>
    inline std::string Format(const std::chrono::system_clock::time_point &tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%H:%M:%S");
        return oss.str();
    }
}
