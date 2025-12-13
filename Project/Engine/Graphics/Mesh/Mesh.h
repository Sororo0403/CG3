#pragma once

#include <vector>
#include <cstdint>

struct Mesh {
    struct Vertex {
        float position[3];
        float normal[3];
        float uv[2];
    };

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
