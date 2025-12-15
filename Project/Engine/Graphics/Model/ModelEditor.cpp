#include "ModelEditor.h"

#include "Mesh/MeshManager.h"
#include "Model/ModelManager.h"
#include "Model/Model.h"
#include "OBJ/OBJManager.h"

#include "imgui/imgui.h"

#include <memory>
#include <cassert>

ModelEditor::ModelEditor(MeshManager *meshManager, ModelManager *modelManager,
                         OBJManager *objManager)
    : meshManager_(meshManager), modelManager_(modelManager),
      objManager_(objManager) {

    assert(meshManager_);
    assert(modelManager_);
    assert(objManager_);
}

void ModelEditor::DrawImGui() {
    ImGui::Begin("Model Editor");

    // =====================================
    // Create Primitive
    // =====================================
    if (ImGui::Button("Create Cube")) {
        CreateCubeModel();
    }

    ImGui::Separator();

    // =====================================
    // Create OBJ
    // =====================================
    ImGui::Text("OBJ Path");
    ImGui::InputText("##ObjPath", objPath_, sizeof(objPath_));

    if (ImGui::Button("Create OBJ")) {
        CreateObjModel(objPath_);
    }

    // =====================================
    // Transform
    // =====================================
    if (currentModelId_ != 0) {
        Model *model = modelManager_->GetModel(currentModelId_);
        if (model) {
            ImGui::Separator();
            ImGui::Text("Transform");

            ImGui::DragFloat3("Position", &model->transform.position.x, 0.1f);

            ImGui::DragFloat3("Rotation", &model->transform.rotation.x, 0.05f);

            ImGui::DragFloat3("Scale", &model->transform.scale.x, 0.1f);
        }
    }

    ImGui::End();
}

void ModelEditor::CreateCubeModel() {
    auto mesh = std::make_unique<Mesh>();

    // ============================
    // Vertex
    // ============================
    mesh->vertices = {
        {{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 1}},
        {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0}},
        {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0}},
        {{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 1}},

        {{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
        {{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
        {{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
    };

    mesh->indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};

    uint32_t meshId = meshManager_->Register(std::move(mesh));
    currentModelId_ = modelManager_->Create(meshId);

    Model *model = modelManager_->GetModel(currentModelId_);
    model->transform.position = {0, 0, 0};
    model->transform.scale = {1, 1, 1};
}

void ModelEditor::CreateObjModel(const std::string &path) {
    currentModelId_ = objManager_->CreateModel(
        path, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
}
