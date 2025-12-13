#pragma once
#include <cstdint>
#include "Transform2D.h"
#include "Vector2.h"
#include "Vector4.h"

struct Sprite {
    uint32_t textureId = 0;

    Transform2D transform;
    Vector2 size{1.0f, 1.0f};

    Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};

    Vector4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};
};
