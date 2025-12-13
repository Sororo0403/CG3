#pragma once

struct Vector2 {
  float x = 0.0f;
  float y = 0.0f;

  Vector2() = default;
  Vector2(float x_, float y_) : x(x_), y(y_) {}

  // 演算子
  Vector2 operator+(const Vector2 &rhs) const { return {x + rhs.x, y + rhs.y}; }
  Vector2 operator-(const Vector2 &rhs) const { return {x - rhs.x, y - rhs.y}; }
  Vector2 operator*(float s) const { return {x * s, y * s}; }
  Vector2 operator/(float s) const { return {x / s, y / s}; }

  Vector2 &operator+=(const Vector2 &rhs) {
    x += rhs.x;
    y += rhs.y;
    return *this;
  }
};
