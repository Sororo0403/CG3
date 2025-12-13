#pragma once

struct Vector4 {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  float w = 0.0f;

  Vector4() = default;
  Vector4(float x_, float y_, float z_, float w_)
      : x(x_), y(y_), z(z_), w(w_) {}

  Vector4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }

  Vector4 operator+(const Vector4 &r) const {
    return {x + r.x, y + r.y, z + r.z, w + r.w};
  }
};
