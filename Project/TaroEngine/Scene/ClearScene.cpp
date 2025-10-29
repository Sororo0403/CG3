#define NOMINMAX
#include "ClearScene.h"

#include "Input.h"
#include "ModelRenderer.h"
#include "GameScene.h"
#include "StageSelectScene.h"

#include "imgui/imgui.h"
#include <format>

using namespace DirectX;

void ClearScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engineContext_ = engine;
    renderContext_ = render;
    sceneManager_ = engineContext_->sceneManager;

    // カメラはとりあえず固定の正射影で「スコア画面」的に
    auto *dx = engineContext_->directXCommon;
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    float aspect = w / std::max(1.0f, h);

    camera_.Initialize(
        {0,0,-10},
        {0,0,0},
        60.0f,
        aspect,
        0.1f,
        100.0f
    );
    camera_.SetOrtho(w, h, 0.1f, 100.0f);
    camera_.LookAt({0,0,-10}, {0,0,0});
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

void ClearScene::Update(float /*dt*/) {
    auto *in = engineContext_->input;

    // 上下でメニュー移動
    if (in->IsKeyPressed(DIK_W)) menuIndex_ = 0;
    if (in->IsKeyPressed(DIK_S)) menuIndex_ = 1;

    // 決定
    if (in->IsKeyPressed(DIK_SPACE)) {
        if (menuIndex_ == 0) {
            // 次のステージへ (同じ難易度で)
            sceneManager_->ChangeScene(
                std::make_unique<GameScene>(nextStageId_, difficulty_)
            );
            return;
        } else {
            // ステージセレクトへ戻る（開始ステージ＆難易度を引き継ぐ）
            sceneManager_->ChangeScene(
                std::make_unique<StageSelectScene>(playedStageId_, difficulty_)
            );
            return;
        }
    }
}


void ClearScene::Draw() {


    // ImGuiで文字を出す（今のデバッグ段階ではこれが一番速い）
    ImGui::Begin("Stage Clear", nullptr,
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_AlwaysAutoResize);

    // クリアタイム表示
    // mm:ss.mmm にする
    int totalMs = (int)(clearTimeSec_ * 1000.0f + 0.5f);
    int minutes = totalMs / 60000;
    int msLeft = totalMs % 60000;
    int seconds = msLeft / 1000;
    int millis = msLeft % 1000;

    ImGui::Text("CLEAR TIME");
    ImGui::Text("%02d:%02d.%03d", minutes, seconds, millis);

    ImGui::Separator();

    // メニュー
    ImGui::Text("%s Next Stage",
        (menuIndex_ == 0) ? ">" : " ");
    ImGui::Text("%s Stage Select",
        (menuIndex_ == 1) ? ">" : " ");

    ImGui::TextDisabled("W/S : Select   SPACE : Confirm");

    ImGui::End();
}

void ClearScene::Finalize() {
    // いまのところ特になし
}
