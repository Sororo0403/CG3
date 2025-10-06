#pragma once

#include "LogMessage.h"

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
    /// メッセージを出力
    /// </summary>
    virtual void Log(const LogMessage &msg) = 0;
};