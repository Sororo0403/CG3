#pragma once

class Input;
class DirectXCommon;
class SceneManager;

/// <summary>
/// エンジン共有コンテキスト。
/// </summary>
struct EngineContext {
	Input *input = nullptr;
	DirectXCommon *directXCommon = nullptr;
	SceneManager *sceneManager = nullptr;
};