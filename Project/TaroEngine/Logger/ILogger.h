#pragma once
#include "LogLevel.h"
#include <string>

/// <summary>
/// ロガーの共通インターフェイス。
/// </summary>
class ILogger {
public:
    /// <summary>
    /// デストラクタ。
    /// </summary>
    virtual ~ILogger() = default;

    /// <summary>
    /// ログレベルを指定してメッセージを出力します。
    /// </summary>
    /// <param name="level">ログレベル。</param>
    /// <param name="msg">出力するメッセージ文字列。</param>
    virtual void Log(LogLevel level, const std::string &msg) = 0;
};
