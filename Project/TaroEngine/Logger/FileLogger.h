#pragma once
#include "ILogger.h"
#include "LogTimeUtil.h"
#include "LogLevelUtil.h"
#include <fstream>
#include <mutex>
#include <string>

/// <summary>
/// ログをファイルに書き出すロガー。
/// </summary>
class FileLogger : public ILogger {
public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    /// <param name="filePath">出力先のファイルパス。</param>
    explicit FileLogger(const std::string &filePath);

    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~FileLogger() override;

    /// <summary>
    /// 指定レベルでログを出力します。
    /// </summary>
    /// <param name="level">ログレベル。</param>
    /// <param name="msg">出力するメッセージ。</param>
    void Log(LogLevel level, const std::string &msg) override;

    /// <summary>
    /// ファイルが正常に開けているかを確認します。
    /// </summary>
    /// <returns>開けている場合 true。</returns>
    bool IsOpen() const;

private:
    std::ofstream ofs_;
    std::mutex mutex_;
    std::string filePath_;
};
