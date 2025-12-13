#pragma once

#include <cstdint>

#include "Transform.h"

/// <summary>
/// 3D モデルのインスタンスデータ
/// </summary>
struct Model {
    Transform transform{};
    uint32_t meshId = 0;
};
