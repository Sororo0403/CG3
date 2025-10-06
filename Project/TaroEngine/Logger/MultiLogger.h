#pragma once
#include "ILogger.h"
#include <vector>
#include <memory>
#include <mutex>

/// <summary>
/// 複数の ILogger に一括でメッセージを配信するロガー。
/// </summary>
class MultiLogger : public ILogger {
public:
    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~MultiLogger() override = default;

    /// <summary>
    /// ロガーを追加します。
    /// </summary>
    /// <param name="logger">登録するロガー。</param>
    void AddLogger(const std::shared_ptr<ILogger> &logger);

    /// <summary>
    /// すべての登録ロガーにメッセージを送ります（ログレベル指定あり）。
    /// </summary>
    /// <param name="level">ログレベル。</param>
    /// <param name="msg">出力するメッセージ。</param>
    void Log(LogLevel level, const std::string &msg) override;

private:
    std::vector<std::shared_ptr<ILogger>> loggers_;
    std::mutex mutex_;
};
