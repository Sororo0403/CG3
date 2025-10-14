#pragma once
#include <string_view>

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL,
};

class ILogger {
public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    virtual ~ILogger() = default;

    /// <summary>
    /// 指定されたログレベルとメッセージを出力
    /// </summary>
    /// <param name="logLevel">出力するログの重大度を示すレベル</param>
    /// <param name="message">出力するメッセージ文字列</param>
    virtual void Log(LogLevel logLevel, std::string_view message) = 0;
};
