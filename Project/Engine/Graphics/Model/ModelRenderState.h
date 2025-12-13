#pragma once

#include <cstdint>

/// <summary>
/// Model の描画状態
/// </summary>
struct ModelRenderState {
    bool visible = true;
    uint32_t layer = 0;
};
