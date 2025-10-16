#pragma once

class DirectXCommon;

/// <summary>
/// エンジン共有コンテキスト。
/// </summary>
struct EngineContext {
	DirectXCommon *directXCommon = nullptr;
};