#pragma once

#include "Matrix4x4.h"

struct TransformationMatrix {
  Matrix4x4 WVP;   ///< ワールド×ビュー×射影
  Matrix4x4 World; ///< ワールド行列
};