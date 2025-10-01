#include "GameScene.h"
#include "imgui.h"   // ★ IMGUI

void GameScene::Initialize(const EngineContext &engine) {
    // --- カメラ初期化 ---
    camera_.Initialize(1280.0f, 720.0f, camFovDeg_ * 3.14159265f / 180.0f, camNear_, camFar_);
    camera_.SetPosition(camPos_);
    camera_.SetTarget(camTarget_);
    camera_.Update();

    // --- スプライト初期化 ---
    sprite_.Initialize(engine.device);
}

void GameScene::OnResize(uint32_t w, uint32_t h) {
    // ビューポート変更（DirectXCommon の Resize 後などに呼んでね）
    camera_.SetViewportSize(static_cast<float>(w), static_cast<float>(h));
}

void GameScene::Update(float /*dt*/) {
     // カメラ行列更新
    camera_.Update();

    // スプライト更新（カメラの ViewProjection を使用する版）
    sprite_.Update(camera_);
}

void GameScene::Draw(const EngineContext &engine, const RenderContext &rc) {
    // ==== ImGui: Camera パネル ====
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
            // Near/Far は常に整合性を保つ
            if (camNear_ < 0.0001f) camNear_ = 0.0001f;
            if (camFar_ <= camNear_ + 0.001f) camFar_ = camNear_ + 0.001f;
            camera_.SetLens(camFovDeg_ * 3.14159265f / 180.0f, camera_.GetAspect(), camNear_, camFar_);
        }

        // 簡易オービット（注視点中心に公転）
        static float orbitYawDeg = 0.0f;
        static float orbitPitchDeg = 0.0f;
        static float orbitRadius = 8.0f;

        ImGui::SeparatorText("Orbit");
        bool orbitChanged = false;
        orbitChanged |= ImGui::DragFloat("Yaw (deg)", &orbitYawDeg, 0.25f, -360.0f, 360.0f);
        orbitChanged |= ImGui::DragFloat("Pitch (deg)", &orbitPitchDeg, 0.25f, -89.0f, 89.0f);
        orbitChanged |= ImGui::DragFloat("Radius", &orbitRadius, 0.05f, 0.01f, 1e6f);

        if (ImGui::Button("Apply Orbit")) {
            // カメラ位置を target 周りに回す（Camera::OrbitTarget は増分式なので小刻みに呼ぶ想定）
            // ここでは度→ラジアン変換して一回適用
            float yawRad = orbitYawDeg * 3.14159265f / 180.0f;
            float pitchRad = orbitPitchDeg * 3.14159265f / 180.0f;

            // 現在の target を使って公転させる
            camera_.SetTarget(camTarget_);
            camera_.OrbitTarget(yawRad, pitchRad, orbitRadius);
            camPos_ = camera_.GetPosition(); // UI 側へ反映
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

    // 共通 PSO / ルートシグネチャ適用
    engine.spriteCommon->ApplyCommonDrawSettings(
        rc.cmdList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // スプライト描画
    sprite_.Draw(rc.cmdList);
}

void GameScene::Finalize() {
    // 特に解放処理なし（必要なら追加）
}
