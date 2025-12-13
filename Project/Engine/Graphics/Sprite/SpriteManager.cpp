#include "SpriteManager.h"

#include <algorithm>
#include <cassert>
#include <vector>

#include "SpriteRenderer.h"
#include "Logger/Logger.h"

SpriteManager::SpriteManager(SpriteRenderer *renderer) : renderer_(renderer) {
  assert(renderer_);
  LOG_INFO("SpriteManager: Created.");
}

uint32_t SpriteManager::Create(uint32_t textureId, Layer layer) {
  uint32_t id = nextId_++;

  Entry entry{};
  entry.sprite.textureId = textureId;
  entry.layer = layer;

  sprites_.emplace(id, std::move(entry));

  LOG_DEBUG("SpriteManager: Sprite created. id=" + std::to_string(id));
  return id;
}

void SpriteManager::Destroy(uint32_t id) {
  auto it = sprites_.find(id);
  if (it == sprites_.end()) {
    LOG_WARN("SpriteManager: Destroy failed. Invalid id.");
    return;
  }

  sprites_.erase(it);
}

Sprite *SpriteManager::Get(uint32_t id) {
  auto it = sprites_.find(id);
  if (it == sprites_.end()) {
    return nullptr;
  }
  return &it->second.sprite;
}

void SpriteManager::Begin() { renderer_->Begin(); }

void SpriteManager::DrawAll() {
  assert(renderer_);

  // 描画用リストを作る）
  std::vector<std::pair<Sprite *, Layer>> drawList;
  drawList.reserve(sprites_.size());

  for (auto &[id, entry] : sprites_) {
    drawList.emplace_back(&entry.sprite, entry.layer);
  }

  // レイヤ順で安定ソート
  std::stable_sort(drawList.begin(), drawList.end(),
                   [](const auto &a, const auto &b) {
                     return static_cast<uint32_t>(a.second) <
                            static_cast<uint32_t>(b.second);
                   });

  for (auto &[sprite, layer] : drawList) {
    renderer_->Draw(*sprite);
  }
}
