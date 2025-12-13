#pragma once
#include "Vector2.h"

struct Transform2D {
    Vector2 position{0.0f, 0.0f};
    float z = 0.0f;

    Vector2 scale{1.0f, 1.0f};
    float rotation = 0.0f;

    Vector2 pivot{0.5f, 0.5f};
};
