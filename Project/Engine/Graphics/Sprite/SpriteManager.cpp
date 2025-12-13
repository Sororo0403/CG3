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

uint32_t SpriteManager::Create(uint32_t textureId, SpriteLayer layer) {
  uint32_t id = nextId_++;

  Entry entry{};
  entry.sprite.textureId = textureId;
  entry.render.layer = layer;
  entry.render.visible = true;

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

void SpriteManager::Begin() { renderer_->Begin(); }

void SpriteManager::DrawAll() {
  assert(renderer_);

  // Entry* で描画リストを作る（render情報も使うため）
  std::vector<Entry *> drawList;
  drawList.reserve(sprites_.size());

  for (auto &[id, entry] : sprites_) {
    drawList.push_back(&entry);
  }

  // Layer 順で安定ソート
  std::stable_sort(drawList.begin(), drawList.end(),
                   [](const Entry *a, const Entry *b) {
                     return static_cast<uint32_t>(a->render.layer) <
                            static_cast<uint32_t>(b->render.layer);
                   });

  for (auto *entry : drawList) {
    if (!entry->render.visible) {
      continue;
    }
    renderer_->Draw(entry->sprite);
  }
}

Sprite *SpriteManager::GetSprite(uint32_t id) {
  auto it = sprites_.find(id);
  return (it != sprites_.end()) ? &it->second.sprite : nullptr;
}

SpriteLayer SpriteManager::GetLayer(uint32_t id) const {
  auto it = sprites_.find(id);
  return (it != sprites_.end()) ? it->second.render.layer : SpriteLayer::UI;
}

bool SpriteManager::IsVisible(uint32_t id) const {
  auto it = sprites_.find(id);
  return (it != sprites_.end()) ? it->second.render.visible : false;
}

void SpriteManager::SetVisible(uint32_t id, bool visible) {
  auto it = sprites_.find(id);
  if (it != sprites_.end()) {
    it->second.render.visible = visible;
  }
}

void SpriteManager::SetLayer(uint32_t id, SpriteLayer layer) {
  auto it = sprites_.find(id);
  if (it != sprites_.end()) {
    it->second.render.layer = layer;
  }
}
