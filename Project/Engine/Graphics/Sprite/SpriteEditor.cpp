#include "SpriteEditor.h"

#include "SpriteManager.h"
#include "Sprite.h"
#include "SpriteRenderState.h"
#include "SpriteLayer.h"
#include "SpriteLayerUtil.h"

#include "Texture/TextureEditor.h"

#include "imgui/imgui.h"

#include <string>
#include <cassert>
#include <vector>

SpriteEditor::SpriteEditor(SpriteManager *spriteManager,
                           TextureEditor *textureEditor)
    : spriteManager_(spriteManager), textureEditor_(textureEditor) {
    assert(spriteManager_);
    assert(textureEditor_);
}

void SpriteEditor::Initialize() {
    selectedId_ = 0;
}

void SpriteEditor::DrawImGui() {
    if (!spriteManager_) {
        return;
    }

    ImGui::Begin("Sprite Editor");

    // ----------------------------------
    // 2カラム（Hierarchy / Inspector）
    // ----------------------------------
    ImGui::Columns(2, nullptr, true);

    // ==================================
    // Hierarchy
    // ==================================
    ImGui::TextUnformatted("Hierarchy");
 
    if (ImGui::Button("Save")) {
        spriteManager_->SaveToJson("sprites.json");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        spriteManager_->LoadFromJson("sprites.json");
    }

    ImGui::Separator();

    if (ImGui::Button("+ Create")) {
        selectedId_ = spriteManager_->Create(1, SpriteLayer::UI);
    }

    ImGui::Separator();

    spriteManager_->ForEach([&](uint32_t id, Sprite &,
                                SpriteRenderState &render) {
        bool selected = (id == selectedId_);

        std::string label =
            "[" +
            std::string(SpriteLayerUtil::SpriteLayerToString(render.layer)) +
            "] Sprite " + std::to_string(id);

        if (ImGui::Selectable(label.c_str(), selected)) {
            selectedId_ = id;
        }
    });

    ImGui::NextColumn();

    // ==================================
    // Inspector
    // ==================================
    ImGui::TextUnformatted("Inspector");
    ImGui::Separator();

    Sprite *sprite = spriteManager_->GetSprite(selectedId_);
    if (!sprite) {
        ImGui::TextDisabled("No sprite selected.");
        ImGui::Columns(1);
        ImGui::End();
        return;
    }

    // RenderState
    bool visible = spriteManager_->IsVisible(selectedId_);
    if (ImGui::Checkbox("Visible", &visible)) {
        spriteManager_->SetVisible(selectedId_, visible);
    }

    SpriteLayer layer = spriteManager_->GetLayer(selectedId_);
    int layerIndex = static_cast<int>(layer);

    if (ImGui::Combo("Layer", &layerIndex,
                     SpriteLayerUtil::GetSpriteLayerNames(),
                     static_cast<int>(SpriteLayer::COUNT))) {
        spriteManager_->SetLayer(selectedId_,
                                 static_cast<SpriteLayer>(layerIndex));
    }

    // Transform
    auto &t = sprite->transform;

    ImGui::SeparatorText("Transform");
    ImGui::DragFloat2("Position", &t.position.x, 1.0f);
    ImGui::DragFloat("Z", &t.z, 0.1f);
    ImGui::DragFloat2("Scale", &t.scale.x, 0.01f);
    ImGui::DragFloat("Rotation", &t.rotation, 0.01f);
    ImGui::SliderFloat2("Pivot", &t.pivot.x, 0.0f, 1.0f);

    // Sprite
    ImGui::SeparatorText("Sprite");
    ImGui::DragFloat2("Size", &sprite->size.x, 1.0f, 0.0f, 10000.0f);
    ImGui::ColorEdit4("Color", &sprite->color.x);
    ImGui::DragFloat4("UV Rect", &sprite->uvRect.x, 0.001f, 0.0f, 1.0f);

    // Texture
    ImGui::SeparatorText("Texture");

    std::vector<TextureEditor::Entry> textures;
    textureEditor_->GetEntries(textures);

    if (textures.empty()) {
        ImGui::TextDisabled("No textures available.");
    } else {
        // 現在の textureId を SpriteManager 経由で取得
        uint32_t currentTexId = spriteManager_->GetTexture(selectedId_);

        int currentIndex = 0;
        for (size_t i = 0; i < textures.size(); ++i) {
            if (textures[i].textureId == currentTexId) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }

        // Combo 用ラベル配列
        std::vector<const char *> labels;
        labels.reserve(textures.size());
        for (auto &e : textures) {
            labels.push_back(e.name.c_str());
        }

        if (ImGui::Combo("Texture", &currentIndex, labels.data(),
                         static_cast<int>(labels.size()))) {
            spriteManager_->SetTexture(selectedId_,
                                       textures[currentIndex].textureId);
        }
    }

    // Delete
    ImGui::Separator();
    if (ImGui::Button("Delete")) {
        spriteManager_->Destroy(selectedId_);
        selectedId_ = 0;
    }

    ImGui::Columns(1);
    ImGui::End();
}
