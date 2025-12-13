#include "SpriteManager.h"

#include <algorithm>
#include <cassert>
#include <vector>
#include <fstream>

#include "nlohmann/json.hpp"

#include "SpriteRenderer.h"
#include "SpriteLayerUtil.h"
#include "Logger/Logger.h"

using json = nlohmann::json;

SpriteManager::SpriteManager(SpriteRenderer *renderer) : renderer_(renderer) {
    assert(renderer_);
    LOG_INFO("SpriteManager: Created.");
}

// ==================================================
// Lifecycle
// ==================================================

uint32_t SpriteManager::Create(uint32_t textureId, SpriteLayer layer) {
    uint32_t id = nextId_++;

    Entry entry{};
    entry.render.layer = layer;
    entry.render.visible = true;

    entry.sprite.textureId = textureId;
    entry.sprite.size = {256.0f, 256.0f};
    entry.sprite.transform.position = {0.0f, 0.0f};
    entry.sprite.transform.scale = {1.0f, 1.0f};
    entry.sprite.transform.pivot = {0.0f, 0.0f};
    entry.sprite.transform.rotation = 0.0f;
    entry.sprite.transform.z = 0.0f;

    sprites_.emplace(id, std::move(entry));
    dirty_ = true;

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
    dirty_ = true;
}

void SpriteManager::Clear() {
    sprites_.clear();
    nextId_ = 1;
    dirty_ = true;
}

void SpriteManager::Begin() {
    renderer_->Begin();
}

void SpriteManager::DrawAll() {
    assert(renderer_);

    std::vector<Entry *> drawList;
    drawList.reserve(sprites_.size());

    for (auto &[id, entry] : sprites_) {
        drawList.push_back(&entry);
    }

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

// ==================================================
// JSON
// ==================================================

bool SpriteManager::SaveToJson(const std::string &path) {
    json root;
    root["sprites"] = json::array();

    ForEach([&](uint32_t id, const Sprite &sprite,
                const SpriteRenderState &render) {
        json j;

        j["id"] = id;
        j["visible"] = render.visible;
        j["layer"] = SpriteLayerUtil::SpriteLayerToString(render.layer);
        j["textureId"] = sprite.textureId;

        const auto &t = sprite.transform;
        j["transform"] = {{"position", {t.position.x, t.position.y}},
                          {"z", t.z},
                          {"scale", {t.scale.x, t.scale.y}},
                          {"rotation", t.rotation},
                          {"pivot", {t.pivot.x, t.pivot.y}}};

        j["sprite"] = {
            {"size", {sprite.size.x, sprite.size.y}},
            {"color",
             {sprite.color.x, sprite.color.y, sprite.color.z, sprite.color.w}},
            {"uvRect",
             {sprite.uvRect.x, sprite.uvRect.y, sprite.uvRect.z,
              sprite.uvRect.w}}};

        root["sprites"].push_back(j);
    });

    std::ofstream ofs(path);
    if (!ofs) {
        LOG_ERROR("SpriteManager: Failed to open file for save.");
        return false;
    }

    ofs << root.dump(4);
    dirty_ = false;

    LOG_INFO("SpriteManager: Saved to " + path);
    return true;
}

bool SpriteManager::LoadFromJson(const std::string &path) {
    std::ifstream ifs(path);
    if (!ifs) {
        LOG_ERROR("SpriteManager: Failed to open file for load.");
        return false;
    }

    json root;
    ifs >> root;

    Clear();

    for (auto &j : root["sprites"]) {
        uint32_t id = Create(0, SpriteLayerUtil::StringToSpriteLayer(
                                    j["layer"].get<std::string>()));

        SetVisible(id, j["visible"].get<bool>());
        SetTexture(id, j["textureId"].get<uint32_t>());

        Sprite *sprite = GetSprite(id);
        auto &t = sprite->transform;

        auto &jt = j["transform"];
        t.position.x = jt["position"][0];
        t.position.y = jt["position"][1];
        t.z = jt["z"];
        t.scale.x = jt["scale"][0];
        t.scale.y = jt["scale"][1];
        t.rotation = jt["rotation"];
        t.pivot.x = jt["pivot"][0];
        t.pivot.y = jt["pivot"][1];

        auto &js = j["sprite"];
        sprite->size.x = js["size"][0];
        sprite->size.y = js["size"][1];

        sprite->color.x = js["color"][0];
        sprite->color.y = js["color"][1];
        sprite->color.z = js["color"][2];
        sprite->color.w = js["color"][3];

        sprite->uvRect.x = js["uvRect"][0];
        sprite->uvRect.y = js["uvRect"][1];
        sprite->uvRect.z = js["uvRect"][2];
        sprite->uvRect.w = js["uvRect"][3];
    }

    dirty_ = false;

    LOG_INFO("SpriteManager: Loaded from " + path);
    return true;
}

// ==================================================
// Setter / Getter
// ==================================================

void SpriteManager::SetVisible(uint32_t id, bool visible) {
    auto it = sprites_.find(id);
    if (it != sprites_.end()) {
        it->second.render.visible = visible;
        dirty_ = true;
    }
}

void SpriteManager::SetLayer(uint32_t id, SpriteLayer layer) {
    auto it = sprites_.find(id);
    if (it != sprites_.end()) {
        it->second.render.layer = layer;
        dirty_ = true;
    }
}

void SpriteManager::SetTexture(uint32_t id, uint32_t textureId) {
    auto it = sprites_.find(id);
    if (it == sprites_.end()) {
        LOG_WARN("SpriteManager: SetTexture failed. Invalid id.");
        return;
    }

    it->second.sprite.textureId = textureId;
    dirty_ = true;
}

Sprite *SpriteManager::GetSprite(uint32_t id) {
    auto it = sprites_.find(id);
    return (it != sprites_.end()) ? &it->second.sprite : nullptr;
}

SpriteLayer SpriteManager::GetLayer(uint32_t id) const {
    auto it = sprites_.find(id);
    return (it != sprites_.end()) ? it->second.render.layer : SpriteLayer::UI;
}

uint32_t SpriteManager::GetTexture(uint32_t id) const {
    auto it = sprites_.find(id);
    return (it != sprites_.end()) ? it->second.sprite.textureId : 0;
}

bool SpriteManager::IsVisible(uint32_t id) const {
    auto it = sprites_.find(id);
    return (it != sprites_.end()) ? it->second.render.visible : false;
}
