#pragma once

class Input;
class DirectXCommon;

/// <summary>
/// エンジン共有コンテキスト。
/// </summary>
struct EngineContext {
	Input *input = nullptr;
	DirectXCommon *directXCommon = nullptr;
};