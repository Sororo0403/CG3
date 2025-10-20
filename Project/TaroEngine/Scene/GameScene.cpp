// GameScene.cpp
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

    camera_.Reset();
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // 初期色
    color_[0] = 1.0f; color_[1] = 1.0f; color_[2] = 1.0f; color_[3] = 1.0f;
}

void GameScene::Update(float deltaTime) {
    if (autoSpin_) angleY_ += deltaTime * rotSpeed_;
    world_ = XMMatrixRotationY(angleY_);
}

void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    // =========================
    // ImGui UI
    // =========================
    if (ImGui::Begin("Scene ▶ Controls")) {
        ImGui::TextDisabled("Camera");
        // 編集用に一旦コピー
        auto eye = camera_.GetEye();
        auto tgt = camera_.GetTarget();
        auto up = camera_.GetUp();
        float fov = camera_.GetFovYDeg();
        float zn = camera_.GetNearZ();
        float zf = camera_.GetFarZ();

        if (ImGui::DragFloat3("Eye", &eye.x, 0.01f))    camera_.SetEye(eye);
        if (ImGui::DragFloat3("Target", &tgt.x, 0.01f)) camera_.SetTarget(tgt);
        if (ImGui::DragFloat3("Up", &up.x, 0.01f))      camera_.SetUp(up);

        bool lensChanged = false;
        lensChanged |= ImGui::SliderFloat("FOV (deg)", &fov, 10.0f, 120.0f, "%.1f");
        lensChanged |= ImGui::DragFloatRange2("ZNear/ZFar", &zn, &zf, 0.01f, 0.01f, 1000.0f, "N=%.2f", "F=%.1f");
        if (lensChanged) {
            camera_.SetPerspective(fov, camera_.GetAspect(), zn, zf);
        }

        if (ImGui::Button("Reset Camera")) {
            camera_.Reset();
            camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
        }

        ImGui::Separator();
        ImGui::TextDisabled("Model");
        ImGui::Checkbox("Auto Spin", &autoSpin_);
        ImGui::DragFloat("Rot Speed (rad/s)", &rotSpeed_, 0.01f, -10.0f, 10.0f);
        ImGui::ColorEdit4("Color", color_, ImGuiColorEditFlags_Float);
        if (ImGui::Button("Reset Rotation")) angleY_ = 0.0f;

        ImGui::Separator();
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();

    // 実ウィンドウのアスペクトを反映
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // 転置行列を取得して描画
    float vT[16], pT[16], wT[16];
    Camera::StoreT(wT, world_);
    camera_.GetTransposeVP(vT, pT);

    model_.Begin(cmd, vT, pT);
    model_.Draw(cmd, cube_, wT, color_);
    model_.End(cmd);
}

void GameScene::Finalize() {
    model_.Finalize();
}
