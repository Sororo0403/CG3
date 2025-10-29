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

// 1枚の板モデルを描画する小ヘルパ
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
    t.pos = pos;                        // {x,y,z}
    t.scale = scale;                      // {sx,sy,sz}
    t.rot = {0.0f, 0.0f, Deg(rotZDeg)}; // Z回転だけでいい

    renderer->Draw(cmd, m, t, alphaMul);
}

void ClearScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engineContext_ = engine;
    renderContext_ = render;
    sceneManager_ = engineContext_->sceneManager;

    auto *dx = engineContext_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // ===== カメラを「スクリーンUI用」に戻す =====
    // 画面解像度をそのまま正射影に突っ込んで、
    // 中心(0,0)で扱いやすいようにする
    {
        float w = (float)dx->GetWidth();
        float h = (float)dx->GetHeight();
        float aspect = w / std::max(1.0f, h);

        camera_.Initialize(
            {0.0f, 0.0f, -10.0f}, // eye
            {0.0f, 0.0f,  0.0f},  // target
            60.0f,
            aspect,
            0.1f,
            1000.0f
        );

        // Orthoを「画面っぽい座標」に:
        //   横幅 = w, 高さ = h
        //   → 左端 ~-w/2, 右端 ~+w/2
        //   → 下 ~-h/2, 上 ~+h/2
        camera_.SetOrtho(w, h, 0.1f, 1000.0f);

        // LookAt を (0,0,-10)→(0,0,0)
        camera_.LookAt({0.0f, 0.0f, -10.0f},
            {0.0f, 0.0f,   0.0f});

        camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    }

    // ===== モデル読み込み =====
    // (パスはスクショ通り Resources/Model/)
    titlePlateModel_.Initialize(device, "Resources/Model/normal.obj");       // 仮: CLEAR TIME欄
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

    spaceModel_.Initialize(device, "Resources/Model/space.obj"); // ":" "." もこれで代用
}

void ClearScene::Update(float /*dt*/) {
    auto *in = engineContext_->input;

    if (in->IsKeyPressed(DIK_W)) menuIndex_ = 0;
    if (in->IsKeyPressed(DIK_S)) menuIndex_ = 1;

    if (in->IsKeyPressed(DIK_SPACE)) {
        if (menuIndex_ == 0) {
            sceneManager_->ChangeScene(
                std::make_unique<GameScene>(nextStageId_, difficulty_));
        } else {
            sceneManager_->ChangeScene(
                std::make_unique<StageSelectScene>(playedStageId_, difficulty_));
        }
    }
}

std::string ClearScene::BuildTimeString_() const {
    // "mm:ss.mmm"
    int totalMs = (int)(clearTimeSec_ * 1000.0f + 0.5f);
    int minutes = totalMs / 60000;
    int msLeft = totalMs % 60000;
    int seconds = msLeft / 1000;
    int millis = msLeft % 1000;
    return std::format("{:02d}:{:02d}.{:03d}", minutes, seconds, millis);
}

// 文字列1文字ずつモデル配置
void ClearScene::DrawTimeString3D_(
    ID3D12GraphicsCommandList *cmd,
    const std::string &timeText,
    float baseX, float baseY, float z,
    float charW, float charH,
    float spacing
) {
    auto *renderer = renderContext_->modelRenderer;

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
            continue;
        }

        float x = originX + (float)i * spacing;

        XMFLOAT3 pos = {x, baseY, z};
        XMFLOAT3 scale = {charW, charH, 0.2f};

        DrawPlate(renderer, cmd, *m, pos, scale, 0.0f, 1.0f);
    }
}

void ClearScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *renderer = renderContext_->modelRenderer;

    renderer->Begin(cmd, dx, camera_);

    // 画面中心を (0,0)
    // 上にタイトル、真ん中にタイム、下にメニュー

    // タイトル (CLEAR TIME的な看板 / normal.obj 代用)
    {
        XMFLOAT3 pos = {0.0f, 150.0f, 0.0f};
        XMFLOAT3 scale = {300.0f, 80.0f, 10.0f};
        DrawPlate(renderer, cmd, titlePlateModel_, pos, scale, 0.0f, 1.0f);
    }

    // タイム文字列
    {
        std::string t = BuildTimeString_();
        DrawTimeString3D_(
            cmd,
            t,
            0.0f,     // baseX
            50.0f,    // baseY
            0.0f,     // z
            40.0f,    // charW
            60.0f,    // charH
            45.0f     // spacing
        );
    }

    // メニュー項目1: NEXT STAGE
    {
        float scaleMul = (menuIndex_ == 0) ? 1.2f : 1.0f;

        XMFLOAT3 pos = {0.0f, -80.0f, 0.0f};
        XMFLOAT3 scale = {260.0f * scaleMul, 70.0f * scaleMul, 10.0f};

        DrawPlate(renderer, cmd, nextStageModel_, pos, scale, 0.0f, 1.0f);
    }

    // メニュー項目2: STAGE SELECT
    {
        float scaleMul = (menuIndex_ == 1) ? 1.2f : 1.0f;

        XMFLOAT3 pos = {0.0f, -180.0f, 0.0f};
        XMFLOAT3 scale = {300.0f * scaleMul, 70.0f * scaleMul, 10.0f};

        DrawPlate(renderer, cmd, stageSelectModel_, pos, scale, 0.0f, 1.0f);
    }

    renderer->End(cmd);
}

void ClearScene::Finalize() {
    // とくに無し
}
