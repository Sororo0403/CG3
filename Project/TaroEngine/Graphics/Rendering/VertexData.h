#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"

struct VertexData {
  Vector4 position; ///< 座標
  Vector2 texcoord; ///< UV
  Vector3 normal;   ///< 法線
};
