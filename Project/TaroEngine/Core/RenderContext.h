#pragma once

#include <d3d12.h>

/// <summary>
/// 描画コンテキスト。
/// </summary>
struct RenderContext {
    ID3D12GraphicsCommandList *commandList = nullptr;
};
