#pragma once

#include <cstdint>

class SpriteManager;

class SpriteEditor {
public:
  SpriteEditor(SpriteManager *spriteManager);

  void Initialize();
  void DrawImGui();

private:
  SpriteManager *spriteManager_ = nullptr;
  uint32_t selectedId_ = 0;
};
