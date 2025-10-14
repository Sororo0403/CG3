#include "OutputLogger.h"

void OutputLogger::Log(LogLevel logLevel, std::string_view message) {
    // 共通ユーティリティで整形されたログ文字列を取得
    std::string formatted = LogUtility::Format(logLevel, message);

    // 出力ウィンドウに改行付きで出力
    formatted += '\n';
    OutputDebugStringA(formatted.c_str());
}
