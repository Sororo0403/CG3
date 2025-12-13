#pragma once
#include <cstdint>

struct Sprite {
  uint32_t textureId = 0;

  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;

  float width = 1.0f;
  float height = 1.0f;

  // radian
  float rotation = 0.0f;

  float pivotX = 0.5f;
  float pivotY = 0.5f;

  float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  float uvRect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
};
