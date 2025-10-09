#include "GameScene.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "imgui.h"

// ImGui が SRV[0] を使用している想定 → スプライトは [1] に置く
namespace { constexpr UINT kSpriteSrvStartIndex = 1; }

void GameScene::Initialize(const EngineContext *engineContext) {
    // === スプライト初期化 ===
    sprite_.Initialize(engineContext->device);

    // 画面サイズ（ピクセル）
    const uint32_t defaultW = 1280;
    const uint32_t defaultH = 720;
    sprite_.SetViewportSize(defaultW, defaultH);

    // 色と矩形設定
    sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    sprite_.SetRect(uiX_, uiY_, uiW_, uiH_);

    // === TextureManager 準備（SRV ヒープと関連付け）===
    texMgr_.Initialize(
        engineContext->device,
        engineContext->directXCommon->GetSrvHeap(),
        kSpriteSrvStartIndex);

    // テクスチャは最初の Draw タイミングでロード（cmd があるとき）
    spriteTex_.reset();
}

void GameScene::Update(float /*deltaTime*/) {
    // 定数バッファ（行列）更新
    sprite_.Update();
}

void GameScene::Draw(const EngineContext *engineContext, const RenderContext *renderContext) {
    // === 初回のみテクスチャをロード & セット ===
    if (!spriteTex_.has_value()) {
        spriteTex_ = texMgr_.Load(renderContext->commandList, L"Resources/uvChecker.png");
        // SetTexture ではなく、SRV を直接設定
        sprite_.SetTextureView(spriteTex_->view);
        // もしくは: sprite_.SetTextureHandle(spriteTex_->view.gpu);
    }

    // === ImGui デバッグUI ===
    if (ImGui::Begin("Sprite")) {
        bool moved = ImGui::DragFloat2("Pos (px)", &uiX_, 1.0f);
        bool sized = ImGui::DragFloat2("Size (px)", &uiW_, 1.0f, 1.0f, 4096.0f);
        bool recol = ImGui::ColorEdit4("Color", uiCol_);

        if (moved || sized) { sprite_.SetRect(uiX_, uiY_, uiW_, uiH_); }
        if (recol) { sprite_.SetColor(uiCol_[0], uiCol_[1], uiCol_[2], uiCol_[3]); }
        ImGui::End();
    }

    // === スプライト描画 ===
    // 共通 PSO とルートシグネチャ適用
    engineContext->spriteCommon->ApplyCommonDrawSettings(
        renderContext->commandList,
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 描画
    sprite_.Draw(renderContext->commandList);
}

void GameScene::Finalize() {
    spriteTex_.reset();
}
