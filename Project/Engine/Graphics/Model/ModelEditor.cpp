#include "ModelEditor.h"

#include "Mesh/MeshManager.h"
#include "ModelManager.h"
#include "Model.h"

#include "imgui/imgui.h"

#include <memory>

ModelEditor::ModelEditor(MeshManager *meshManager, ModelManager *modelManager)
    : meshManager_(meshManager), modelManager_(modelManager) {
}

void ModelEditor::DrawImGui() {
    ImGui::Begin("Model Editor");

    if (ImGui::Button("Create Cube Model")) {
        CreateCubeModel();
    }

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
    // Vertex（24頂点：6面 × 4）
    // ============================
    mesh->vertices = {
        // ---- Front (+Z) ----
        {{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 1}},
        {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0}},
        {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0}},
        {{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 1}},

        // ---- Back (-Z) ----
        {{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
        {{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
        {{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},

        // ---- Left (-X) ----
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
        {{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
        {{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 0}},
        {{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {1, 1}},

        // ---- Right (+X) ----
        {{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 1}},
        {{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0}},
        {{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
        {{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}},

        // ---- Top (+Y) ----
        {{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 1}},
        {{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
        {{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0}},
        {{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 1}},

        // ---- Bottom (-Y) ----
        {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}},
        {{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0, 0}},
        {{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0}},
        {{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}},
    };

    // ============================
    // Index（6面 × 2三角形）
    // ============================
    mesh->indices = {
        0,  1,  2,  0,  2,  3, // Front
        4,  5,  6,  4,  6,  7, // Back
        8,  9,  10, 8,  10, 11, // Left
        12, 13, 14, 12, 14, 15, // Right
        16, 17, 18, 16, 18, 19, // Top
        20, 21, 22, 20, 22, 23 // Bottom
    };

    // ============================
    // Mesh 登録
    // ============================
    uint32_t meshId = meshManager_->Register(std::move(mesh));

    // ============================
    // Model 作成
    // ============================
    currentModelId_ = modelManager_->Create(meshId);

    Model *model = modelManager_->GetModel(currentModelId_);
    model->transform.position = {0.0f, 0.0f, 5.0f};
    model->transform.rotation = {0.0f, 0.0f, 0.0f};
    model->transform.scale = {1.0f, 1.0f, 1.0f};
}
