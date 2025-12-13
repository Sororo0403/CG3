#pragma once

#include <cstdint>

/// <summary>
/// Sprite描画レイヤ。
/// 値が小さいほど先に描画される。
/// </summary>
enum class SpriteLayer : uint32_t {
  BACKGROUND = 0,
  WORLD,
  UI,
  FADE,
  DEBUG,

  COUNT
};
