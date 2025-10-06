#pragma once
#include <d3d12.h>

/// <summary>
/// フレームごとの描画用コンテキスト。
/// </summary>
struct RenderContext {
	ID3D12GraphicsCommandList *commandList = nullptr; // コマンドリスト
};
