#pragma once

#include "Matrix4x4.h"

struct TransformMatrix {
  Matrix4x4 WVP;   ///< ワールド×ビュー×射影
  Matrix4x4 World; ///< ワールド行列
};