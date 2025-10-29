#define NOMINMAX
#include "ClearScene.h"

#include "Input.h"
#include "ModelRenderer.h"
#include "GameScene.h"
#include "StageSelectScene.h"
#include "DirectXCommon.h"

#include <format>
#include <algorithm>
#include <DirectXMath.h>

using namespace DirectX;

static inline float Deg(float d) {
    return XMConvertToRadians(d);
}

// 共通の板描画ヘルパ
static void DrawPlate(
    ModelRenderer *renderer,
    ID3D12GraphicsCommandList *cmd,
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &scale,
    float rotZDeg,
    float alphaMul = 1.0f
) {
    Transform t{};
    t.pos = pos;
    t.scale = scale;
    t.rot = {0.0f, 0.0f, Deg(rotZDeg)};

    renderer->Draw(cmd, m, t, alphaMul);
}

void ClearScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engineContext_ = engine;
    renderContext_ = render;
    sceneManager_ = engineContext_->sceneManager;

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    //
    // ===== カメラをUI用に設定 =====
    //
    {
        // アスペクトを取る
        float w = (float)dx->GetWidth();
        float h = (float)dx->GetHeight();
        float aspect = w / std::max(1.0f, h);

        // カメラ自体は -10 の位置から原点を見る
        camera_.Initialize(
            {0.0f, 0.0f, -10.0f}, // eye
            {0.0f, 0.0f,  0.0f},  // target
            60.0f,
            aspect,
            0.1f,
            1000.0f
        );

        // 正射影行列を
        //  X方向: [-aspect, +aspect]
        //  Y方向: [-1, +1]
        // に固定する (= 解像度に依らないUI座標系)
        float orthoW = aspect * 2.0f; // 幅(左右で2*aspect)
        float orthoH = 2.0f;          // 高さ(上下で2)
        camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);

        camera_.LookAt(
            {0.0f, 0.0f, -10.0f},
            {0.0f, 0.0f,   0.0f}
        );

        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    //
    // ===== モデル読み込み =====
    //
    // NOTE: それぞれの obj はユーザ側のプロジェクトに合わせること
    // "normal.obj" は CLEAR TIME 看板の代わり
    // "nextStage.obj" は "NEXT STAGE"
    // "stageSelect.obj" は "STAGE SELECT"
    // 数字/space はもうある前提
    //
    titlePlateModel_.Initialize(device, "Resources/Model/normal.obj");
    nextStageModel_.Initialize(device, "Resources/Model/nextStage.obj");
    stageSelectModel_.Initialize(device, "Resources/Model/stageSelect.obj");

    digitModel_[0].Initialize(device, "Resources/Model/0.obj");
    digitModel_[1].Initialize(device, "Resources/Model/1.obj");
    digitModel_[2].Initialize(device, "Resources/Model/2.obj");
    digitModel_[3].Initialize(device, "Resources/Model/3.obj");
    digitModel_[4].Initialize(device, "Resources/Model/4.obj");
    digitModel_[5].Initialize(device, "Resources/Model/5.obj");
    digitModel_[6].Initialize(device, "Resources/Model/6.obj");
    digitModel_[7].Initialize(device, "Resources/Model/7.obj");
    digitModel_[8].Initialize(device, "Resources/Model/8.obj");
    digitModel_[9].Initialize(device, "Resources/Model/9.obj");

    spaceModel_.Initialize(device, "Resources/Model/space.obj"); // ":" "." " " もこれで代用
}

void ClearScene::Update(float /*dt*/) {
    auto *in = engineContext_->input;

    // 上下でメニュー選択
    if (in->IsKeyPressed(DIK_W)) {
        menuIndex_ = 0;
    }
    if (in->IsKeyPressed(DIK_S)) {
        menuIndex_ = 1;
    }

    // SPACEで決定
    if (in->IsKeyPressed(DIK_SPACE)) {
        if (menuIndex_ == 0) {
            // NEXT STAGE
            sceneManager_->ChangeScene(
                std::make_unique<GameScene>(nextStageId_, difficulty_));
        } else {
            // STAGE SELECT に戻る
            sceneManager_->ChangeScene(
                std::make_unique<StageSelectScene>(playedStageId_, difficulty_));
        }
    }
}

std::string ClearScene::BuildTimeString_() const {
    // mm:ss.mmm
    int totalMs = (int)(clearTimeSec_ * 1000.0f + 0.5f);
    int minutes = totalMs / 60000;
    int msLeft = totalMs % 60000;
    int seconds = msLeft / 1000;
    int millis = msLeft % 1000;

    return std::format("{:02d}:{:02d}.{:03d}",
        minutes, seconds, millis);
}

// 時間の文字列を1文字ずつ板モデルで描く
void ClearScene::DrawTimeString3D_(
    ID3D12GraphicsCommandList *cmd,
    const std::string &timeText,
    float baseX, float baseY, float z,
    float charW, float charH,
    float spacing
) {
    auto *renderer = renderContext_->modelRenderer;

    // 全体幅から中央寄せする
    float totalW = (float)timeText.size() * spacing;
    float originX = baseX - totalW * 0.5f;

    for (size_t i = 0; i < timeText.size(); ++i) {
        char c = timeText[i];
        Model *m = nullptr;

        if (c >= '0' && c <= '9') {
            m = &digitModel_[c - '0'];
        } else if (c == ':' || c == '.' || c == ' ') {
            m = &spaceModel_;
        } else {
            // 未対応文字はスキップ
            continue;
        }

        float x = originX + (float)i * spacing;

        XMFLOAT3 pos = {x, baseY, z};
        XMFLOAT3 scale = {charW, charH, 0.05f};

        DrawPlate(renderer, cmd, *m, pos, scale, 0.0f, 1.0f);
    }
}

void ClearScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);

    // ===== タイトル (CLEAR TIME 看板のつもり)
    {
        XMFLOAT3 pos = {0.0f,  0.60f, 0.0f};
        XMFLOAT3 scale = {0.80f, 0.18f, 0.05f};

        DrawPlate(renderer, cmd, titlePlateModel_, pos, scale, 0.0f, 1.0f);
    }

    // ===== クリアタイム mm:ss.mmm
    {
        std::string t = BuildTimeString_();

        float baseX = 0.0f;
        float baseY = 0.20f;
        float z = 0.0f;
        float charW = 0.10f; // 横サイズ
        float charH = 0.18f; // 縦サイズ
        float spacing = 0.12f; // 文字間隔

        DrawTimeString3D_(
            cmd,
            t,
            baseX,
            baseY,
            z,
            charW,
            charH,
            spacing
        );
    }

    // ===== メニュー: NEXT STAGE
    {
        // ← 選択中はもっと左へ
        float xOffset = (menuIndex_ == 0) ? -0.5f : -0.2f;
        // 非選択も -0.2f にして全体を左寄せ、選択中だけさらに左に抜ける感じにした

        XMFLOAT3 pos = {xOffset, -0.20f, 0.0f};
        XMFLOAT3 scale = {0.2f,   0.16f,  0.05f};

        DrawPlate(renderer, cmd, nextStageModel_, pos, scale, 0.0f, 1.0f);
    }

    // ===== メニュー: STAGE SELECT
    {
        float xOffset = (menuIndex_ == 1) ? -0.5f : -0.2f;

        XMFLOAT3 pos = {xOffset, -0.45f, 0.0f};
        XMFLOAT3 scale = {0.2f,   0.16f,  0.05f};

        DrawPlate(renderer, cmd, stageSelectModel_, pos, scale, 0.0f, 1.0f);
    }

    renderer->End(cmd);
}

void ClearScene::Finalize() {
    // 特になし
}
