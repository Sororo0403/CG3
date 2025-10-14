#pragma once

#include <d3d12.h>

class DirectXCommon;
class SpriteCommon;
class LoggerManager;

struct EngineContext {
	DirectXCommon *directXCommon = nullptr;
	ID3D12Device *device = nullptr;
	SpriteCommon *spriteCommon = nullptr;
	LoggerManager *loggerManager = nullptr;
};