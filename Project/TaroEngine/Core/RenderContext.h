#pragma once

#include <d3d12.h>

// フレームごとの描画用コンテキスト
struct RenderContext {
	ID3D12GraphicsCommandList *commandList = nullptr; ///< コマンドリスト
};
