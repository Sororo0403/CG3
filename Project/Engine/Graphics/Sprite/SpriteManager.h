#pragma once

#include <unordered_map>
#include <cstdint>

#include "Sprite.h"

class SpriteRenderer;

/// <summary>
/// スプライトの生成・管理・描画順制御・一括描画を行うマネージャ。
/// Sprite はこのクラスが所有する。
/// </summary>
class SpriteManager {
public:
  /// <summary>
  /// 描画レイヤ。
  /// 値が小さいほど先に描画される。
  /// </summary>
  enum class Layer : uint32_t {
    BACKGROUND = 0,
    WORLD,
    UI,
    FADE,
    DEBUG,
  };

public:
  /// <summary>
  /// コンストラクタ。
  /// 使用する SpriteRenderer を注入する。
  /// </summary>
  SpriteManager(SpriteRenderer *renderer);

  /// <summary>
  /// デストラクタ
  /// </summary>
  ~SpriteManager() = default;

  /// <summary>
  /// Sprite を生成して登録する。
  /// </summary>
  uint32_t Create(uint32_t textureId, Layer layer);

  /// <summary>
  /// Sprite を削除する。
  /// </summary>
  void Destroy(uint32_t id);

  /// <summary>
  /// フレーム開始時に呼ぶ。
  /// </summary>
  void Begin();

  /// <summary>
  /// 登録された Sprite をレイヤ順に一括描画する。
  /// </summary>
  void DrawAll();

  /// <summary>
  /// Sprite を取得する（存在しなければ nullptr）。
  /// </summary>
  Sprite *Get(uint32_t id);

private:
  struct Entry {
    Sprite sprite;
    Layer layer = Layer::UI;
  };

private:
  SpriteRenderer *renderer_ = nullptr;

  std::unordered_map<uint32_t, Entry> sprites_;
  uint32_t nextId_ = 1;
};
