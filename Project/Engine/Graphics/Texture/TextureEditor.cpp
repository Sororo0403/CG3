#include "TextureEditor.h"

#include <cassert>
#include <filesystem>

#include "Texture/TextureManager.h"
#include "Texture/TextureDropQueue.h"
#include "Logger/Logger.h"

#include "imgui/imgui.h"

namespace fs = std::filesystem;

TextureEditor::TextureEditor(TextureManager *textureManager,
                             TextureDropQueue *dropQueue)
    : textureManager_(textureManager), dropQueue_(dropQueue) {
    assert(textureManager_);
    assert(dropQueue_);
}

uint32_t TextureEditor::LoadInternal(const std::string &name,
                                     const std::string &path) {
    auto it = nameToId_.find(name);
    if (it != nameToId_.end()) {
        return it->second;
    }

    uint32_t texId = textureManager_->LoadTexture(path);
    nameToId_[name] = texId;

    LOG_INFO("TextureEditor: Registered [" + name + "] -> " + path);
    return texId;
}

void TextureEditor::Update() {
    std::string path;

    while (dropQueue_->Pop(path)) {
        fs::path p(path);
        std::string ext = p.extension().string();

        if (ext != ".png" && ext != ".jpg" && ext != ".jpeg") {
            LOG_WARN("TextureEditor: ignored -> " + path);
            continue;
        }

        std::string name = p.filename().string();
        LoadInternal(name, path);
    }
}

void TextureEditor::DrawImGui() {
    if (!ImGui::Begin("TextureEditor")) {
        ImGui::End();
        return;
    }

    ImGui::Text("Drop .png / .jpg files onto the window");
    ImGui::Separator();

    if (nameToId_.empty()) {
        ImGui::TextDisabled("No textures loaded.");
    }

    // 一覧表示
    for (auto it = nameToId_.begin(); it != nameToId_.end();) {
        ImGui::PushID(it->first.c_str());

        ImGui::Text("%s", it->first.c_str());
        ImGui::SameLine();

        ImGui::TextDisabled("(id=%u)", it->second);
        ImGui::SameLine();

        // 管理から削除（GPUリソースは残す）
        if (ImGui::SmallButton("Remove")) {
            LOG_INFO("TextureEditor: Removed [" + it->first + "]");
            it = nameToId_.erase(it);
            ImGui::PopID();
            continue;
        }

        ImGui::PopID();
        ++it;
    }

    ImGui::Separator();

    if (ImGui::Button("Clear All")) {
        Clear();
    }

    ImGui::End();
}


void TextureEditor::Clear() {
    nameToId_.clear();
}

uint32_t TextureEditor::GetTexture(const std::string &name) const {
    auto it = nameToId_.find(name);
    if (it == nameToId_.end()) {
        return 0;
    }
    return it->second;
}

bool TextureEditor::IsExists(const std::string &name) const {
    return nameToId_.contains(name);
}

void TextureEditor::GetEntries(std::vector<Entry> &out) const {
    out.clear();
    out.reserve(nameToId_.size());

    for (const auto &[name, id] : nameToId_) {
        out.push_back(Entry{name, id});
    }
}
