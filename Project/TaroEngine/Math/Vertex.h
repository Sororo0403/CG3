#pragma once

#include <DirectXMath.h>

struct Vertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 nrm;
    DirectX::XMFLOAT2 uv;
    DirectX::XMFLOAT4 color;
};