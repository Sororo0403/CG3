#pragma once

#include <d3d12.h>

class DirectXCommon;
class LoggerManager;

/// <summary>
/// エンジン共有コンテキスト。
/// </summary>
struct EngineContext {
	DirectXCommon *directXCommon = nullptr;
	LoggerManager *loggerManager = nullptr;
};