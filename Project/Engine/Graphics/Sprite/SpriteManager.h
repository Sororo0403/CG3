#pragma once

#include <unordered_map>
#include <cstdint>
#include "nlohmann/json.hpp"

#include "Sprite.h"
#include "SpriteRenderState.h"

class SpriteRenderer;

/// <summary>
/// スプライトの生成・管理・描画順制御・一括描画を行うマネージャ。
/// Sprite はこのクラスが所有する。
/// </summary>
class SpriteManager {
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
    uint32_t Create(uint32_t textureId, SpriteLayer layer);

    /// <summary>
    /// Sprite を削除する。
    /// </summary>
    void Destroy(uint32_t id);

    /// <summary>
    /// SPrite を全削除する。
    /// </summary>
    void Clear();

    /// <summary>
    /// フレーム開始時に呼ぶ。
    /// </summary>
    void Begin();

    /// <summary>
    /// 登録された Sprite をレイヤ順に一括描画する。
    /// </summary>
    void DrawAll();

    // json
    bool SaveToJson(const std::string &path) const;
    bool LoadFromJson(const std::string &path);

    // Setter
    void SetVisible(uint32_t id, bool visible);
    void SetLayer(uint32_t id, SpriteLayer layer);
    void SetTexture(uint32_t id, uint32_t textureId);

    // Getter
    Sprite *GetSprite(uint32_t id);
    SpriteLayer GetLayer(uint32_t id) const;
    uint32_t GetTexture(uint32_t id) const;
    bool IsVisible(uint32_t id) const;

    // Editor
    template <class Fn> void ForEach(Fn fn) {
        for (auto &[id, entry] : sprites_) {
            fn(id, entry.sprite, entry.render);
        }
    }
    template <class Fn> void ForEach(Fn fn) const {
        for (const auto &[id, entry] : sprites_) {
            fn(id, entry.sprite, entry.render);
        }
    }

  private:
    struct Entry {
        Sprite sprite;
        SpriteRenderState render;
    };

  private:
    SpriteRenderer *renderer_ = nullptr;

    std::unordered_map<uint32_t, Entry> sprites_;
    uint32_t nextId_ = 1;
};
