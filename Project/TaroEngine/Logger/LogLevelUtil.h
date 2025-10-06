#pragma once
#include "LogLevel.h"
#include <string>

/// <summary>
/// ログ関連のユーティリティ関数群。
/// </summary>
namespace LogLevelUtil {

    /// <summary>
    /// ログレベルを文字列に変換します。
    /// </summary>
    /// <param name="level">変換対象のログレベル。</param>
    /// <returns>ログレベルを表す文字列。</returns>
    inline const char *ToString(LogLevel level) {
        switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
        }
    }

    /// <summary>
    /// 文字列からログレベルを取得します。
    /// </summary>
    /// <param name="str">ログレベル名（"TRACE"、"DEBUG"、"INFO" など）。</param>
    /// <returns>対応するログレベル。該当しない場合は <see cref="LogLevel::INFO"/>。</returns>
    inline LogLevel FromString(const std::string &str) {
        if (str == "DEBUG") return LogLevel::DEBUG;
        if (str == "INFO")  return LogLevel::INFO;
        if (str == "WARN")  return LogLevel::WARN;
        if (str == "ERROR") return LogLevel::ERROR;
        return LogLevel::INFO;
    }
}
