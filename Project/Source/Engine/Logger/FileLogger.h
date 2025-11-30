#pragma once
#include "ILogger.h"
#include "LogUtility.h"
#include <fstream>
#include <string>
#include <mutex>

class FileLogger : public ILogger {
public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~FileLogger() override;

    /// <summary>
    /// ファイルパスを設定
    /// </summary>
    /// <param name="filePath">設定するファイルパス</param>
    void SetFilePath(const std::string &filePath);

    /// <summary>
    /// 指定されたログレベルとメッセージをファイルに出力します。
    /// </summary>
    /// <param name="logLevel">出力するログの重大度を示すレベル</param>
    /// <param name="message">出力するメッセージ文字列</param>
    void Log(LogLevel logLevel, std::string_view message)  override;

    /// <summary>
    /// 指定されたログレベルとメッセージをファイルに出力します。
    /// </summary>
    /// <param name="logLevel">出力するログの重大度を示すレベル</param>
    /// <param name="message">出力するメッセージ文字列</param>
    void Log(LogLevel logLevel, std::wstring_view message) override;

private:
    std::ofstream stream_;
    std::mutex mutex_;
};
