#pragma once
#include <d3d12.h>

class DirectXCommon;
class SpriteCommon;

/// <summary>
/// エンジン全体で共有する長寿命オブジェクトを束ねる。
/// </summary>
struct EngineContext {
	DirectXCommon *directXCommon = nullptr; // DirectX12基盤管理
	ID3D12Device *device = nullptr; // D3D12デバイス
	SpriteCommon *spriteCommon = nullptr; // スプライト共通描画設定
};