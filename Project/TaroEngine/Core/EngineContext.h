#pragma once

#include <d3d12.h>
#include <memory>

class DirectXCommon;
class SpriteCommon;
class MultiLogger;

/// <summary>
/// エンジン全体で共有する長寿命オブジェクトを束ねる。
/// </summary>
struct EngineContext {
	DirectXCommon *directXCommon = nullptr; // DirectX12基盤管理
	ID3D12Device *device = nullptr; // D3D12デバイス
	SpriteCommon *spriteCommon = nullptr; // スプライト共通描画設定
	std::unique_ptr<MultiLogger> multiLogger;
};

/// <summary>
/// フレームごとの描画用コンテキスト。
/// </summary>
struct RenderContext {
	ID3D12GraphicsCommandList *commandList = nullptr; // コマンドリスト
};
