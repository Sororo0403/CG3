#define NOMINMAX
#include "GameScene.h"
#include "DirectXCommon.h"
#include <algorithm>
#include "imgui/imgui.h"

using namespace DirectX;

void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;

    auto *dx = engineContext_->directXCommon;
    model_.Initialize(dx->GetDevice(), renderContext_->shaderCompiler);
    cube_.CreateBox(dx->GetDevice());

    // 初期のビュー・プロジェクション（実値は Update/Draw で毎フレーム再計算）
    view_ = XMMatrixLookAtLH(XMLoadFloat3(&eye_), XMLoadFloat3(&tgt_), XMLoadFloat3(&up_));

    const float w = 1280.0f, h = 720.0f;
    proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovYDeg_), w / h, nearZ_, farZ_);

    // モデル色は初期白
    color_[0] = 1.0f; color_[1] = 1.0f; color_[2] = 1.0f; color_[3] = 1.0f;
}

void GameScene::Update(float deltaTime) {
    if (autoSpin_) {
        angleY_ += deltaTime * rotSpeed_;
    }
    world_ = XMMatrixRotationY(angleY_);
}

void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    // =========================
    // ImGui UI（ここでパラメータを編集）
    // =========================
    if (ImGui::Begin("Scene \xef\xbc\xbc Controls")) { // 「Scene ▶ Controls」
        ImGui::TextDisabled("Camera");
        ImGui::DragFloat3("Eye", &eye_.x, 0.01f, -1000.0f, 1000.0f, "%.2f");
        ImGui::DragFloat3("Target", &tgt_.x, 0.01f, -1000.0f, 1000.0f, "%.2f");
        ImGui::DragFloat3("Up", &up_.x, 0.01f, -10.0f, 10.0f, "%.2f");

        ImGui::SliderFloat("FOV (deg)", &fovYDeg_, 10.0f, 120.0f, "%.1f");
        ImGui::DragFloatRange2("ZNear/ZFar", &nearZ_, &farZ_, 0.01f, 0.01f, 1000.0f, "N=%.2f", "F=%.1f");

        if (ImGui::Button("Reset Camera")) {
            eye_ = {0.0f, 1.5f, -3.0f};
            tgt_ = {0.0f, 0.5f,  0.0f};
            up_ = {0.0f, 1.0f,  0.0f};
            fovYDeg_ = 60.0f; nearZ_ = 0.1f; farZ_ = 100.0f;
        }

        ImGui::Separator();
        ImGui::TextDisabled("Model");
        ImGui::Checkbox("Auto Spin", &autoSpin_);
        ImGui::DragFloat("Rot Speed (rad/s)", &rotSpeed_, 0.01f, -10.0f, 10.0f);
        ImGui::ColorEdit4("Color", color_, ImGuiColorEditFlags_Float);

        if (ImGui::Button("Reset Rotation")) { angleY_ = 0.0f; }

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();

    // UI の値で最新の View / Proj を再計算（アスペクトは実ウィンドウから取得）
    const uint32_t wU = std::max<uint32_t>(1u, dx->GetWidth());
    const uint32_t hU = std::max<uint32_t>(1u, dx->GetHeight());
    const float aspect = static_cast<float>(wU) / static_cast<float>(hU);

    view_ = XMMatrixLookAtLH(XMLoadFloat3(&eye_), XMLoadFloat3(&tgt_), XMLoadFloat3(&up_));
    proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovYDeg_), aspect, nearZ_, farZ_);

    // HLSL 既定 column_major → 転置して送る
    float vT[16], pT[16], wT[16];
    StoreT(vT, view_);
    StoreT(pT, proj_);
    StoreT(wT, world_);

    model_.Begin(cmd, vT, pT);
    model_.Draw(cmd, cube_, wT, color_);
    model_.End(cmd);
}

void GameScene::Finalize() {
    model_.Finalize();
}
