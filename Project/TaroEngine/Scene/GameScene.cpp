#include "GameScene.h"
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "imgui.h"
#include <DirectXMath.h>
using namespace DirectX;

// ImGui が SRV[0] を使用している想定 → スプライトは [1] から
namespace { constexpr UINT kSpriteSrvStartIndex = 1; }

// 行列ユーティリティ（Transpose 済みの float[16] を作る簡易版）
static void MakeViewProj_T(float *out16, float eyeX, float eyeY, float eyeZ,
    float tgtX, float tgtY, float tgtZ,
    float upX, float upY, float upZ,
    float fovY, float aspect, float zn, float zf) {
    XMVECTOR eye = XMVectorSet(eyeX, eyeY, eyeZ, 1.0f);
    XMVECTOR tgt = XMVectorSet(tgtX, tgtY, tgtZ, 1.0f);
    XMVECTOR up = XMVectorSet(upX, upY, upZ, 0.0f);
    XMMATRIX V = XMMatrixLookAtRH(eye, tgt, up);
    XMMATRIX P = XMMatrixPerspectiveFovRH(fovY, aspect, zn, zf);
    XMMATRIX VP = XMMatrixTranspose(V * P); // ← シェーダ側の行列読み取りに合わせて転置
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4 *>(out16), VP);
}

void GameScene::Initialize(const EngineContext *engineContext) {
    // === Sprite ===
    sprite_.Initialize(engineContext->device);
    sprite_.SetViewportSize(1280, 720);
    sprite_.SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    sprite_.SetRect(uiX_, uiY_, uiW_, uiH_);

    // === SRV ヒープと紐付け（ImGui[0] を避けて 1 から使う）===
    texMgr_.Initialize(
        engineContext->device,
        engineContext->directXCommon->GetSrvHeap(),
        kSpriteSrvStartIndex);

    spriteTex_.reset();

    // === 3D 共通部 & plane ===
    obj3dCommon_.Initialize(engineContext->directXCommon); // ルートシグネチャ・PSO 構築
    plane_.Initialize(&obj3dCommon_);
    // ※ モデルは Resources/Models/plane.obj を想定（Z+ を上にした右手座標などは環境に合わせて）
    plane_.LoadObj(L"Resources/plane.obj");
    plane_.SetScale(pScl_[0], pScl_[1], pScl_[2]);
    plane_.SetColor(pCol_[0], pCol_[1], pCol_[2], pCol_[3]);
    planeTex_.reset();

    // カメラ行列
    MakeViewProj_T(viewProj_, camPos_[0], camPos_[1], camPos_[2],
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);
}

void GameScene::Update(float /*deltaTime*/) {
    // === plane の状態反映 ===
    plane_.SetPosition(pPos_[0], pPos_[1], pPos_[2]);
    plane_.SetRotation(pRot_[0], pRot_[1], pRot_[2]);
    plane_.SetScale(pScl_[0], pScl_[1], pScl_[2]);
    plane_.SetColor(pCol_[0], pCol_[1], pCol_[2], pCol_[3]);

    // CB 書き込み（viewProj と camera）
    plane_.Update(viewProj_, camPos_);

    // === sprite は定数更新 ===
    sprite_.Update();
}

void GameScene::Draw(const EngineContext *engineContext, const RenderContext *renderContext) {
    // === 初回ロード（遅延）===
    if (!spriteTex_.has_value()) {
        spriteTex_ = texMgr_.Load(renderContext->commandList, L"Resources/uvChecker.png");
        sprite_.SetTextureView(spriteTex_->view);
        // sprite_.SetTextureHandle(spriteTex_->view.gpu); // どちらでもOK
    }
    if (!planeTex_.has_value()) {
        // plane も同じテクスチャをとりあえず使う（必要なら差し替え）
        planeTex_ = texMgr_.Load(renderContext->commandList, L"Resources/uvChecker.png");
        plane_.SetTextureSrv(planeTex_->view.gpu);
    }

    // === ImGui ===
    if (ImGui::Begin("Sprite")) {
        bool moved = ImGui::DragFloat2("Pos (px)", &uiX_, 1.0f);
        bool sized = ImGui::DragFloat2("Size (px)", &uiW_, 1.0f, 1.0f, 4096.0f);
        bool recol = ImGui::ColorEdit4("Color", uiCol_);
        if (moved || sized) { sprite_.SetRect(uiX_, uiY_, uiW_, uiH_); }
        if (recol) { sprite_.SetColor(uiCol_[0], uiCol_[1], uiCol_[2], uiCol_[3]); }
        ImGui::End();
    }
    if (ImGui::Begin("Plane")) {
        ImGui::DragFloat3("Pos", pPos_, 0.05f);
        ImGui::DragFloat3("Rot(rad)", pRot_, 0.01f);
        ImGui::DragFloat3("Scale", pScl_, 0.05f, 0.01f, 100.0f);
        ImGui::ColorEdit4("Color", pCol_);
        ImGui::End();
    }

    // === 3D描画 ===
    // ※ PSO/RS は Object3d 側で共通設定を適用します
    plane_.Draw(renderContext->commandList);

    // === 2Dスプライト描画 ===
    engineContext->spriteCommon->ApplyCommonDrawSettings(
        renderContext->commandList,
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    sprite_.Draw(renderContext->commandList);
}

void GameScene::Finalize() {
    spriteTex_.reset();
    planeTex_.reset();
}
