#pragma once

#include <d3d12.h>

struct RenderContext {
    ID3D12GraphicsCommandList *commandList = nullptr;
};
