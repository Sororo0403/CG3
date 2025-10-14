#pragma once
#include "LogLevel.h"
#include <chrono>
#include <string>
#include <format>

namespace LogUtility {

	/// <summary>
	/// ログレベルを文字列に変換
	/// </summary>
	/// <param name="level">変換するログレベル</param>
	/// <returns>ログレベルを表す文字列</returns>
	inline const char *ToString(LogLevel level) {
		switch (level) {
		case LogLevel::TRACE: return "TRACE";
		case LogLevel::DEBUG: return "DEBUG";
		case LogLevel::INFO:  return "INFO";
		case LogLevel::WARN:  return "WARN";
		case LogLevel::ERR: return "ERROR";
		case LogLevel::FATAL: return "FATAL";
		default:              return "UNKNOWN";
		}
	}

	/// <summary>
	/// 現在時刻を"HH:MM:SS.mmm"形式で取得
	/// </summary>
	/// <returns>フォーマット済みの時刻文字列</returns>
	inline std::string GetTimeStamp() {
		using namespace std::chrono;
		const auto now = system_clock::now();
		const auto time = system_clock::to_time_t(now);
		const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

		std::tm local{};
		localtime_s(&local, &time);

		return std::format("{:02}:{:02}:{:02}.{:03}",
			local.tm_hour, local.tm_min, local.tm_sec, static_cast<int>(ms.count()));
	}

	/// <summary>
	/// ログ出力用の共通フォーマット文字列を生成
	/// </summary>
	/// <param name="level">ログレベル</param>
	/// <param name="message">ログメッセージ</param>
	/// <returns>フォーマット済みのログ文字列</returns>
	inline std::string Format(LogLevel level, std::string_view message) {
		return std::format("[{}][{}] {}", GetTimeStamp(), ToString(level), message);
	}

}
