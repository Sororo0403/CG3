#pragma once

#include <d3d12.h>

class DirectXCommon;
class SpriteCommon;

// エンジン全体で共有する主要コンテキスト
struct EngineContext {
	DirectXCommon *directXCommon = nullptr; ///< DirectX12基盤管理
	ID3D12Device *device = nullptr;			///< D3D12デバイス
	SpriteCommon *spriteCommon = nullptr;   ///< スプライト共通描画設定
};