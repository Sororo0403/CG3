#pragma once

#include "Vector3.h"

/// <summary>
/// 位置・回転・拡縮を表す Transform
/// </summary>
struct Transform {
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 rotation{0.0f, 0.0f, 0.0f};
    Vector3 scale{1.0f, 1.0f, 1.0f};
};
