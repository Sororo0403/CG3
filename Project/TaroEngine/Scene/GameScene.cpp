#define USE_MATH_DEFINES_
#include <cmath>
#include "GameScene.h"
#include "imgui.h"
#include "MultiLogger.h"
#include "LogLevel.h"
#include "SpriteCommon.h"

void GameScene::Initialize(const EngineContext &engine) {
    // --- カメラ初期化（※スプライトには非依存だが、他の描画で使うなら維持） ---
   // const float aspect = 1280.0f / 720.0f;
    camera_.Initialize(1280.0f, 720.0f, camFovDeg_ * 3.14159265f / 180.0f, camNear_, camFar_);
    camera_.SetPosition(camPos_);
    camera_.SetTarget(camTarget_);
    camera_.Update();

    // --- スプライト初期化（画面張り付き） ---
    sprite_.Initialize(engine.device);
    sprite_.SetViewportSize(1280, 720);       // ← 重要：ピクセル→NDC行列に必要
    sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f); // 白（コメントと実値を一致）
    sprite_.SetRect(100.0f, 100.0f, 256.0f, 256.0f); // ピクセル矩形
    engine.multiLogger->Log(LogLevel::INFO, "GameScene: Sprite initialized.");
}

void GameScene::OnResize(uint32_t w, uint32_t h) {
    // --- ビューポート変更（カメラ） ---
    camera_.SetViewportSize(static_cast<float>(w), static_cast<float>(h));

    // --- スプライト側の画面サイズも必ず更新 ---
    sprite_.SetViewportSize(w, h);
}

void GameScene::Update(float /*dt*/) {
    // カメラ行列更新（他用途で使用するなら）
    camera_.Update();

    // スプライト更新（ピクセル→NDC行列などを更新）
    sprite_.Update();
}

void GameScene::Draw(const EngineContext &engine, const RenderContext &rc) {
    // ==== ImGui: Camera パネル（必要なら表示） ====
    if (ImGui::Begin("Camera")) {
        // 位置／注視点
        if (ImGui::DragFloat3("Position", &camPos_.x, 0.05f)) {
            camera_.SetPosition(camPos_);
        }
        if (ImGui::DragFloat3("Target", &camTarget_.x, 0.05f)) {
            camera_.SetTarget(camTarget_);
        }

        // FOV / Near / Far
        if (ImGui::SliderFloat("FOV (deg)", &camFovDeg_, 10.0f, 120.0f)) {
            camera_.SetLens(camFovDeg_ * 3.14159265f / 180.0f, camera_.GetAspect(), camNear_, camFar_);
        }
        bool nearChanged = ImGui::DragFloat("Near", &camNear_, 0.01f, 0.001f, camFar_ - 0.001f);
        bool farChanged = ImGui::DragFloat("Far", &camFar_, 1.0f, camNear_ + 0.001f, 100000.0f);
        if (nearChanged || farChanged) {
            if (camNear_ < 0.0001f) camNear_ = 0.0001f;
            if (camFar_ <= camNear_ + 0.001f) camFar_ = camNear_ + 0.001f;
            camera_.SetLens(camFovDeg_ * 3.14159265f / 180.0f, camera_.GetAspect(), camNear_, camFar_);
        }

        // 簡易オービット（注視点中心に公転）
        static float orbitYawDeg = 0.0f;
        static float orbitPitchDeg = 0.0f;
        static float orbitRadius = 8.0f;

        ImGui::SeparatorText("Orbit");
        ImGui::DragFloat("Yaw (deg)", &orbitYawDeg, 0.25f, -360.0f, 360.0f);
        ImGui::DragFloat("Pitch (deg)", &orbitPitchDeg, 0.25f, -89.0f, 89.0f);
        ImGui::DragFloat("Radius", &orbitRadius, 0.05f, 0.01f, 1e6f);

        if (ImGui::Button("Apply Orbit")) {
            float yawRad = orbitYawDeg * 3.14159265f / 180.0f;
            float pitchRad = orbitPitchDeg * 3.14159265f / 180.0f;
            camera_.SetTarget(camTarget_);
            camera_.OrbitTarget(yawRad, pitchRad, orbitRadius);
            camPos_ = camera_.GetPosition(); // UI へ反映
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            camPos_ = {0.0f, 3.0f, -8.0f};
            camTarget_ = {0.0f, 1.0f,  0.0f};
            camFovDeg_ = 60.0f;
            camNear_ = 0.1f;
            camFar_ = 2000.0f;
            camera_.SetPosition(camPos_);
            camera_.SetTarget(camTarget_);
            camera_.SetLens(camFovDeg_ * 3.14159265f / 180.0f, camera_.GetAspect(), camNear_, camFar_);
        }
        ImGui::End();
    }

    // 共通 PSO / ルートシグネチャ適用（SpriteCommon）
    engine.spriteCommon->ApplyCommonDrawSettings(
        rc.commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // スプライト描画（画面張り付き）
    sprite_.Draw(rc.commandList);
}

void GameScene::Finalize() {
    // 特に解放処理なし（必要なら追加）
}
