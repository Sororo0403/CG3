#include "OutputLogger.h"
#include "LogUtility.h"
#include <Windows.h>

void OutputLogger::Log(LogLevel logLevel, std::string_view message) {
	// 共通ユーティリティで整形されたログ文字列を取得
	std::string formatted = LogUtility::Format(logLevel, message);

	// 出力ウィンドウに改行付きで出力
	formatted += '\n';
	OutputDebugStringA(formatted.c_str());
}

void OutputLogger::Log(LogLevel logLevel, std::wstring_view message) {
	// 共通ユーティリティで整形されたログ文字列を取得
	std::wstring formatted = LogUtility::FormatW(logLevel, message);

	// 出力ウィンドウに改行付きで出力
	formatted += L'\n';
	OutputDebugStringW(formatted.c_str());
}
