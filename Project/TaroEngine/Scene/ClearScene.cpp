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
#include <cmath>

using namespace DirectX;

static inline float Deg(float d) {
    return XMConvertToRadians(d);
}

// 共通の板描画ヘルパ (UIのメニューや数字用)
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
    // ===== カメラをUI + 背景共通で使える形に設定 =====
    //
    {
        // ここは TitleScene と同じ「世界座標ベースの正射影カメラ」に寄せたい。
        // worldW_ x worldH_ のエリアがだいたい画面に収まるようにする。
        //
        float screenW = (float)dx->GetWidth();
        float screenH = (float)dx->GetHeight();
        float aspect = screenW / std::max(1.0f, screenH);
        float sceneAspect = worldW_ / worldH_;

        float orthoW = 0.0f;
        float orthoH = 0.0f;
        if (aspect >= sceneAspect) {
            // 横に余白が出る（縦がピッタリ）
            orthoH = worldH_;
            orthoW = worldH_ * aspect;
        } else {
            // 縦に余白が出る（横がピッタリ）
            orthoW = worldW_;
            orthoH = worldW_ / aspect;
        }

        float camZ = -50.0f;
        float centerX = 0.0f;
        float centerY = worldH_ * 0.5f;

        camera_.Initialize(
            {centerX, centerY, camZ},
            {centerX, centerY, 0.0f},
            60.0f,
            aspect,
            0.1f,
            1000.0f
        );

        camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
        camera_.LookAt(
            {centerX, centerY, camZ},
            {centerX, centerY, 0.0f}
        );
        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    //
    // ===== モデル読み込み =====
    //

    // --- 背景で使うブロック類（TitleSceneと同じ）
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlFragileTop_.Initialize(device, "Resources/Model/Block/fragile_top.obj");
    mdlFragileBottom_.Initialize(device, "Resources/Model/Block/fragile_bottom.obj");
    mdlRegen_.Initialize(device, "Resources/Model/Block/regen.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");

    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");
    mdlSwitchBlockOn_.Initialize(device, "Resources/Model/Block/switchblock_on.obj");
    mdlSwitchBlockOff_.Initialize(device, "Resources/Model/Block/switchblock_off.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");

    // --- UIで使う文字板
    titlePlateModel_.Initialize(device, "Resources/Model/normal.obj");       // CLEAR TIME の代わり
    nextStageModel_.Initialize(device, "Resources/Model/nextStage.obj");     // NEXT STAGE
    stageSelectModel_.Initialize(device, "Resources/Model/stageSelect.obj"); // STAGE SELECT

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

    spaceModel_.Initialize(device, "Resources/Model/space.obj"); // ":" "." " " 用
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
            // STAGE SELECT
            sceneManager_->ChangeScene(
                std::make_unique<StageSelectScene>(playedStageId_, difficulty_));
        }
    }
}

std::string ClearScene::BuildTimeString_() const {
    // mm:ss.mmm 形式
    int totalMs = (int)(clearTimeSec_ * 1000.0f + 0.5f);
    int minutes = totalMs / 60000;
    int msLeft = totalMs % 60000;
    int seconds = msLeft / 1000;
    int millis = msLeft % 1000;

    return std::format("{:02d}:{:02d}.{:03d}",
        minutes, seconds, millis);
}

void ClearScene::DrawTimeString3D_(
    ID3D12GraphicsCommandList *cmd,
    const std::string &timeText,
    float baseX, float baseY, float z,
    float charW, float charH,
    float spacing
) {
    auto *renderer = renderContext_->modelRenderer;

    // 文字列全体を中央寄せしたいので、合計幅からオフセット
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
            continue; // 未対応は飛ばす
        }

        float x = originX + (float)i * spacing;

        XMFLOAT3 pos = {x, baseY, z};
        XMFLOAT3 scale = {charW, charH, 0.05f};

        DrawPlate(renderer, cmd, *m, pos, scale, 0.0f, 1.0f);
    }
}

// TitleScene と同じノリの背景を再現
void ClearScene::DrawBackground_() {
    float W = worldW_;
    float H = worldH_;

    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // 1) 夜空っぽい後ろレイヤ（Spring / JumpOnly をでっかくして色面として使う）
    DrawModel_(
        mdlSpring_,
        {0.0f, H * 0.50f, 38.0f},
        {W * 2.6f, H * 2.2f, 0.25f},
        {0,0,0},
        1.0f
    );

    DrawModel_(
        mdlJumpOnly_,
        {0.0f, H * 0.48f, 37.8f},
        {W * 2.6f, H * 1.9f, 0.25f},
        {0,0,-4.0f},
        1.0f
    );

    for (int i = -3; i <= 3; ++i) {
        float rx = i * (W * 0.35f);
        float ry = H * (0.70f + 0.03f * std::sin(i * 0.9f));
        float randv = float(((i * 31 * 2654435761u) & 255)) / 255.0f;
        float rw = W * Lerp(0.50f, 0.80f, randv);
        float rh = H * 0.06f;
        float rz = 37.6f - (i & 1) * 0.2f;

        DrawModel_(
            mdlSpring_,
            {rx, ry, rz},
            {rw, rh, 0.05f},
            {0,0,(i & 1) ? -8.0f : 10.0f},
            1.0f
        );
    }

    // 2) ビル群
    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax,
        float hMin, float hMax,
        float tiltDeg, bool warnLight) {
            int count = int(W / span) + 8;
            for (int i = -count / 2; i <= count / 2; ++i) {
                float rx = i * span;
                uint32_t seed = (uint32_t)(i * 73 + (int)(z * 10));
                float r = (float)(seed & 0xFFFF) / 65535.0f;

                float rw = Lerp(wMin, wMax, r);
                float rh = Lerp(hMin, hMax, 1.0f - r);

                // ビル本体
                DrawModel_(
                    mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0,0,(i & 1) ? tiltDeg : -tiltDeg},
                    1.0f
                );

                // 屋上の小箱 (SwitchBlockOff)
                DrawModel_(
                    mdlSwitchBlockOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f},
                    1.0f
                );

                // 警告灯っぽい (SwitchBlockOn)
                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(
                        mdlSwitchBlockOn_,
                        {rx, yBase + rh + 0.24f, z - 0.05f},
                        {0.10f, 0.10f, 0.15f},
                        {0,0,0},
                        1.0f
                    );
                }
            }
        };

    DrawBuildings(
        33.0f, H * 0.06f,
        W * 0.14f,
        W * 0.06f, W * 0.10f,
        H * 0.16f, H * 0.30f,
        2.0f, true
    );
    DrawBuildings(
        29.0f, H * 0.08f,
        W * 0.12f,
        W * 0.07f, W * 0.12f,
        H * 0.18f, H * 0.36f,
        3.0f, true
    );
    DrawBuildings(
        25.0f, H * 0.10f,
        W * 0.10f,
        W * 0.08f, W * 0.14f,
        H * 0.22f, H * 0.40f,
        4.0f, false
    );

    // 3) クレーン・ライト・吊り荷
    DrawModel_(
        mdlJumpOnly_,
        {-W * 0.32f, H * 0.86f, 24.8f},
        {0.06f, H * 0.55f, 0.30f},
        {0,0,0},
        1.0f
    );

    DrawModel_(
        mdlSolid_,
        {-W * 0.06f, H * 1.03f, 24.6f},
        {W * 0.55f, 0.06f, 0.30f},
        {0,0,-9.0f},
        1.0f
    );

    DrawModel_(
        mdlSolid_,
        {W * 0.22f, H * 0.88f, 24.5f},
        {0.035f,  H * 0.28f, 0.25f},
        {0,0,0},
        1.0f
    );

    DrawModel_(
        mdlSwitchOff_,
        {W * 0.22f, H * 0.72f, 24.4f},
        {0.14f, 0.14f, 0.22f},
        {0,0,0},
        1.0f
    );

    DrawModel_(
        mdlSolid_,
        {W * 0.22f, H * 0.55f, 24.3f},
        {0.35f, 0.08f, 0.25f},
        {0,0,4.0f},
        1.0f
    );

    DrawModel_(
        mdlSwitchOn_,
        {W * 0.22f, H * 0.47f, 24.2f},
        {0.15f, 0.08f, 0.22f},
        {0,0,0},
        1.0f
    );

    // 4) 手すり＆注意テープ（手前）
    {
        float railY = -0.6f;

        DrawModel_(
            mdlSolid_,
            {0.0f, railY, -0.40f},
            {W * 0.66f, 0.05f, 0.22f},
            {0,0,0},
            1.0f
        );

        for (int i = -3; i <= 3; ++i) {
            float x = i * (W * 0.16f);
            DrawModel_(
                mdlSolid_,
                {x, railY + 0.20f, -0.41f},
                {W * 0.09f, 0.03f, 0.22f},
                {0,0,(i % 2 == 0) ? -10.0f : 12.0f},
                1.0f
            );
        }

        for (int i = -2; i <= 2; ++i) {
            DrawModel_(
                mdlJumpOnly_,
                {i * (W * 0.18f), railY + 0.55f, -0.42f},
                {W * 0.08f, 0.01f, 0.2f},
                {0,0, 10.0f * std::sinf(float(i))},
                1.0f
            );
        }
    }

    // 5) 両サイド投光器
    auto Flood = [&](XMFLOAT3 b, float rotZDeg) {
        DrawModel_(
            mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0},
            1.0f
        );

        DrawModel_(
            mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0, rotZDeg},
            1.0f
        );

        DrawModel_(
            mdlSpike_,
            {b.x + 0.10f, b.y + 0.26f, 21.7f},
            {W * 0.22f, H * 0.10f, 0.05f},
            {0,0, rotZDeg - 12.0f},
            1.0f
        );
        DrawModel_(
            mdlSpike_,
            {b.x + 0.08f, b.y + 0.28f, 21.6f},
            {W * 0.24f, H * 0.11f, 0.05f},
            {0,0, rotZDeg - 14.0f},
            1.0f
        );
        };

    Flood({-W * 0.48f, H * 0.82f, 0}, 10.0f);
    Flood({W * 0.52f, H * 0.74f, 0}, 18.0f);
}

// ModelRenderer 経由で1つのモデルを実際に描く（TitleSceneと同じスケールのやり方）
void ClearScene::DrawModel_(
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &fullScale,
    const XMFLOAT3 &rotDeg,
    float alphaMul
) {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = renderContext_->modelRenderer;

    Transform tr{};
    tr.pos = pos;
    // あなたのエンジンが「1.0が直径2.0扱い」っぽいので0.5掛けはTitleSceneに合わせて維持
    tr.scale = {
        fullScale.x * 0.5f,
        fullScale.y * 0.5f,
        fullScale.z * 0.5f
    };
    tr.rot = {
        Deg(rotDeg.x),
        Deg(rotDeg.y),
        Deg(rotDeg.z)
    };

    mr->Draw(cmd, m, tr, alphaMul);
}

void ClearScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);

    //
    // 1) まず背景を描く（工事現場の夜景）
    //
    DrawBackground_();

    //
    // 2) その上に UI を重ねる
    //    ここでは world座標ベースで位置を決める
    //    worldH_は18くらいなので、Y=worldH_*0.6とかにすると画面上部に来る
    //

    // CLEAR TIME 的なタイトル
    {
        XMFLOAT3 pos = {0.0f, worldH_ * 0.80f, 0.0f};
        XMFLOAT3 scale = {4.5f, 1.0f, 0.05f};
        DrawPlate(renderer, cmd, titlePlateModel_, pos, scale, 0.0f, 1.0f);
    }

    // クリアタイム (mm:ss.mmm)
    {
        std::string t = BuildTimeString_();

        float baseX = 0.0f;
        float baseY = worldH_ * 0.60f;
        float z = 0.0f;
        float charW = 0.6f;
        float charH = 1.0f;
        float spacing = 0.7f;

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

    // メニュー: NEXT STAGE
    {
        // 選択中はもっと左へ
        float xOffset = (menuIndex_ == 0) ? -10.0f : -4.0f;

        XMFLOAT3 pos = {xOffset, worldH_ * 0.35f, 0.0f};
        XMFLOAT3 scale = {2.0f, 1.0f, 0.05f};

        DrawPlate(renderer, cmd, nextStageModel_, pos, scale, 0.0f, 1.0f);
    }

    // メニュー: STAGE SELECT
    {
        float xOffset = (menuIndex_ == 1) ? -10.0f : -4.0f;

        XMFLOAT3 pos = {xOffset, worldH_ * 0.20f, 0.0f};
        XMFLOAT3 scale = {2.0f, 1.0f, 0.05f};

        DrawPlate(renderer, cmd, stageSelectModel_, pos, scale, 0.0f, 1.0f);
    }

    renderer->End(cmd);
}

void ClearScene::Finalize() {
    // 今のところ特に後始末なし
}
