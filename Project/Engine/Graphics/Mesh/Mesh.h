#pragma once

#include <vector>
#include <cstdint>

#include "Vector2.h"
#include "Vector3.h"

struct Mesh {
    struct Vertex {
        Vector3 position;
        Vector3 normal;
        Vector2 uv;
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
