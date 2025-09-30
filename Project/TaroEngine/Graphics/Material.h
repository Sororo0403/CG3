#pragma once

#include "Matrix4x4.h"
#include "Vector4.h"
#include <cstdint>

struct Material {
  Vector4 color;          ///< 色
  int32_t enableLighting; ///< ライティング有効フラグ
  float padding[3];       ///< 16byte アライメント調整用
  Matrix4x4 uvTransform;  ///< UV 変換行列
};