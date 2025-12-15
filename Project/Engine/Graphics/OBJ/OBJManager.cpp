#include "ObjManager.h"

#include "Mesh/MeshManager.h"
#include "Model/ModelManager.h"
#include "Model/Model.h"
#include "OBJLoader.h"

#include <cassert>

OBJManager::OBJManager(MeshManager *meshManager, ModelManager *modelManager)
    : meshManager_(meshManager), modelManager_(modelManager) {

    assert(meshManager_);
    assert(modelManager_);
}

uint32_t OBJManager::CreateModel(const std::string &path) {
    // Mesh キャッシュ確認
    uint32_t meshId = 0;
    auto it = meshCache_.find(path);
    if (it != meshCache_.end()) {
        meshId = it->second;
    } else {
        meshId = meshManager_->RegisterFromOBJ(path);
        meshCache_[path] = meshId;
    }

    // Model 作成
    uint32_t modelId = modelManager_->Create(meshId);
    return modelId;
}

uint32_t OBJManager::CreateModel(const std::string &path,
                                 const Vector3 &position,
                                 const Vector3 &rotation,
                                 const Vector3 &scale) {

    uint32_t modelId = CreateModel(path);

    Model *model = modelManager_->GetModel(modelId);
    assert(model);

    model->transform.position = position;
    model->transform.rotation = rotation;
    model->transform.scale = scale;

    return modelId;
}
