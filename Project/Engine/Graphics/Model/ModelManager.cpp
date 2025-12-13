#include "ModelManager.h"

#include <cassert>
#include <vector>
#include <algorithm>

#include "Mesh/MeshManager.h"
#include "ModelRenderer.h"
#include "Logger/Logger.h"

ModelManager::ModelManager(ModelRenderer *renderer, MeshManager *meshManager)
    : renderer_(renderer), meshManager_(meshManager) {
    assert(renderer_);
    assert(meshManager_);
    LOG_INFO("ModelManager: Created.");
}

uint32_t ModelManager::Create(uint32_t meshId) {
    // Mesh の存在チェック（CPU Mesh）
    assert(meshManager_->GetMesh(meshId));

    Entry entry{};
    entry.model.meshId = meshId;
    entry.render.visible = true;
    entry.render.layer = 0;

    uint32_t id = nextId_++;
    models_.emplace(id, std::move(entry));

    LOG_DEBUG("ModelManager: Model created. id=" + std::to_string(id));
    return id;
}

void ModelManager::Destroy(uint32_t id) {
    models_.erase(id);
}

void ModelManager::Clear() {
    models_.clear();
    nextId_ = 1;
}

void ModelManager::Begin() {
    renderer_->Begin();
}

void ModelManager::DrawAll(const Camera *camera) {
    assert(renderer_);

    std::vector<Entry *> drawList;
    drawList.reserve(models_.size());

    for (auto &[id, entry] : models_) {
        drawList.push_back(&entry);
    }

    std::stable_sort(drawList.begin(), drawList.end(),
                     [](const Entry *a, const Entry *b) {
                         return a->render.layer < b->render.layer;
                     });

    for (auto *entry : drawList) {
        if (!entry->render.visible) {
            continue;
        }
        renderer_->Draw(entry->model,camera);
    }
}

void ModelManager::SetVisible(uint32_t id, bool visible) {
    auto it = models_.find(id);
    if (it != models_.end()) {
        it->second.render.visible = visible;
    }
}

bool ModelManager::IsVisible(uint32_t id) const {
    auto it = models_.find(id);
    return (it != models_.end()) ? it->second.render.visible : false;
}

void ModelManager::SetLayer(uint32_t id, uint32_t layer) {
    auto it = models_.find(id);
    if (it != models_.end()) {
        it->second.render.layer = layer;
    }
}

uint32_t ModelManager::GetLayer(uint32_t id) const {
    auto it = models_.find(id);
    return (it != models_.end()) ? it->second.render.layer : 0;
}

Model *ModelManager::GetModel(uint32_t id) {
    auto it = models_.find(id);
    return (it != models_.end()) ? &it->second.model : nullptr;
}
