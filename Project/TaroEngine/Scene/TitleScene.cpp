#define NOMINMAX
#include "TitleScene.h"

#include <algorithm>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>

#include "DirectXCommon.h"
#include "ModelRenderer.h"
#include "BufferUtility.h"
#include "Input.h"
#include "SceneManager.h"

#include <DirectXTex/DirectXTex.h>
#include <DirectXTex/d3dx12.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ===== UTF-8 → UTF-16 =====
static std::wstring Widen(const std::string &s) {
    if (s.empty()) return L"";
    int wlen = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), wlen);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

// ===== 決定性ありのちょい乱数 (0..1) =====
static float Hash01(int n) {
    uint32_t h = (uint32_t)(n) * 2654435761u;
    h ^= (h >> 13); h *= 0x5bd1e995u; h ^= (h >> 15);
    return (h & 0xFFFFFF) / float(0xFFFFFF);
}

// =======================================================
// Initialize
// =======================================================
void TitleScene::Initialize(const EngineContext *engine, const RenderContext *render) {
    engine_ = engine;
    render_ = render;

    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();

    // === モデル読込（Block ディレクトリの既存ファイル名に一致） ===
    mdlSolid_.Initialize(device, "Resources/Model/Block/solid.obj");
    mdlFragileAny_.Initialize(device, "Resources/Model/Block/fragile_any.obj");
    mdlJumpOnly_.Initialize(device, "Resources/Model/Block/jumponly.obj");
    mdlSpike_.Initialize(device, "Resources/Model/Block/spike.obj");
    mdlSpring_.Initialize(device, "Resources/Model/Block/spring.obj");
    mdlSwitch_.Initialize(device, "Resources/Model/Block/switch.obj");
    mdlSwitchOn_.Initialize(device, "Resources/Model/Block/switch_on.obj");
    mdlSwitchOff_.Initialize(device, "Resources/Model/Block/switch_off.obj");

    // === テクスチャSRV割り当て（OBJの map_Kd をSRV化してモデルに渡す） ===
    auto setupTex = [&](Model &m, UINT slot, ComPtr<ID3D12Resource> &holder) {
        if (m.GetAlbedoPath().empty()) return;
        D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
        if (LoadTextureSRV_(Widen(m.GetAlbedoPath()), slot, holder, gpu)) {
            m.SetAlbedoSRV(gpu);
        }
        };
    setupTex(mdlSolid_, kSrv_T_Solid, texSolid_);
    setupTex(mdlFragileAny_, kSrv_T_FragileAny, texFragileAny_);
    setupTex(mdlJumpOnly_, kSrv_T_JumpOnly, texJumpOnly_);
    setupTex(mdlSpike_, kSrv_T_Spike, texSpike_);
    setupTex(mdlSpring_, kSrv_T_Spring, texSpring_);
    setupTex(mdlSwitch_, kSrv_T_Switch, texSwitch_);
    setupTex(mdlSwitchOn_, kSrv_T_SwitchOn, texSwitchOn_);
    setupTex(mdlSwitchOff_, kSrv_T_SwitchOff, texSwitchOff_);

    // === カメラ（ゲームシーンと同条件 / 正射影カメラ） ===
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    float aspect = std::max(1.0f, w) / std::max(1.0f, h);
    float orthoW = virtualWorldH_ * aspect;
    float orthoH = virtualWorldH_;

    // カメラ初期化
    // Zはマイナスが奥、プラスが手前というレイアウト想定
    camera_.Initialize(
        {0.0f, orthoH * 0.5f, -50.0f}, // eye
        {0,0,0},                       // (後でLookAtで上書き)
        60.0f, aspect, 0.1f, 1000.0f
    );
    camera_.SetOrtho(orthoW, orthoH, 0.1f, 1000.0f);
    camera_.LookAt(
        {0.0f, orthoH * 0.5f, -50.0f},
        {0.0f, orthoH * 0.5f,   0.0f}
    );
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    blinkTime_ = 0.0f;
    blinkStrength_ = 0.0f;
}

// =======================================================
void TitleScene::Finalize() {
    // ComPtr による自動解放に任せる
}

// =======================================================
// 正射影サイズ更新
// =======================================================
void TitleScene::RefreshCameraOrtho_() {
    auto *dx = engine_->directXCommon;
    float w = (float)dx->GetWidth();
    float h = (float)dx->GetHeight();
    if (w <= 0 || h <= 0) return;

    float aspect = w / std::max(1.0f, h);
    if (camera_.IsOrtho()) {
        camera_.SetOrthoViewSize(virtualWorldH_ * aspect, virtualWorldH_);
    }
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
}

// =======================================================
// Update
// =======================================================
void TitleScene::Update(float dt) {
    RefreshCameraOrtho_();

    // 時間の経過
    blinkTime_ += dt;

    // ゆるい呼吸みたいな明滅カーブをつくる
    // basePulse: 0..1を往復するsin
    float basePulse = 0.5f * (1.0f + std::sinf(blinkTime_ * 3.0f));
    // powで先端だけ残して「ふっと光る/すっと消える」感じに
    blinkStrength_ = std::pow(basePulse, 3.0f); // 0..1 (だいたい0寄りで、たまに明るい)

    // Spaceでシーン遷移
    if (engine_->input && engine_->input->IsKeyTriggered(DIK_SPACE)) {
        // TODO: 実際は StageSelectScene とかに差し替えたい
        engine_->sceneManager->ChangeScene(std::make_unique<TitleScene>());
    }
}

// =======================================================
// DrawModel_
// fullScale = {幅,高さ,奥行き} をそのまま渡すと
//   内部で半サイズにして Transform.scale に入れる。
// OBJが±1を想定してるときに、簡単に「このくらいの大きさの板を置きたい」
// が書けるようにしている。
// =======================================================
void TitleScene::DrawModel_(
    Model &m,
    const XMFLOAT3 &pos,
    const XMFLOAT3 &fullScale,
    const XMFLOAT3 &rotDeg) {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    auto Deg = [](float d) { return XMConvertToRadians(d); };

    Transform t{};
    t.pos = pos;
    t.scale = {
        fullScale.x * 0.5f,
        fullScale.y * 0.5f,
        fullScale.z * 0.5f
    }; // 直径指定→半径スケールに
    t.rot = {
        Deg(rotDeg.x),
        Deg(rotDeg.y),
        Deg(rotDeg.z)
    };

    mr->Draw(cmd, m, t);
}

// =======================================================
// 内部用：幅w, 高さh の「板」パーツを (cx, cy, z) に置く
// rotDegZ だけ簡単に傾けられる
// thickness は固定で0.05fぐらいの薄板想定
// =======================================================
static void DrawRectPart(
    TitleScene *self,
    Model &blockModel,
    const DirectX::XMFLOAT3 &center,
    float w, float h,
    float z,
    float rotDegZ = 0.0f) {
    self->DrawModel_(
        blockModel,
        {center.x, center.y, z},
        {w, h, 0.05f},     // 奥行きは固定の薄板扱い
        {0.0f, 0.0f, rotDegZ}
    );
}

// =======================================================
// "SPACE" のサインを描画
// baseW / baseH … 1文字あたりの基本スケール
//
// やってること：
//
// ・文字は S,P,A,C,E
// ・各文字は「ベースの鉄骨」「上にかぶる光帯(ハイライト)」の2レイヤ
//   - ベースは mdlFragileAny_ で常に見える
//   - 光帯は blinkStrength_ を使ってふわっと膨張＋手前に出す
//     →呼吸するネオンサインっぽい「淡い点滅」
// =======================================================
void TitleScene::DrawSpaceSign_(float baseW, float baseH) {
    float H = virtualWorldH_;

    // 画面中央より少し下あたりに表示
    float originX = 0.0f;
    float originY = H * 0.20f;
    float zBase = -0.6f; // 手すりと同じくらいの手前レイヤ

    // 文字どうしの間隔
    float glyphSpan = baseW * 1.2f;

    // ========= 点滅制御に使うパラメータ =========
    // 上下にほんの少し揺れる
    float glowScaleMul = 1.0f + blinkStrength_ * 0.12f; // 0.06f → 0.12f にして揺れを大きめに
    float glowZAdd = 0.02f + blinkStrength_ * 0.03f; // ちょい手前に出しやすく
    float subtleFloat = (blinkStrength_ - 0.5f) * 0.06f; // 上下揺れもほんの少し増やす

    // --- ベースバー描画（常に見える） ---
    auto DrawBarBase = [&](float cx, float cy,
        float w, float h,
        float rotDegZ = 0.0f) {
            DrawRectPart(this, mdlFragileAny_,
                {cx, cy, zBase},
                w, h, zBase,
                rotDegZ);
        };

    // --- 光帯（ハイライト）描画 ---
    //   ベースの上に少しだけ膨らませた板を、手前に・少し上下に揺らして描く
  // --- 光帯（ハイライト）描画 ---
// mdlSwitchOn_ = 明るい黄色ライト系の見た目を想定
    auto DrawBarGlow = [&](float cx, float cy,
        float w, float h,
        float rotDegZ = 0.0f) {
            DrawRectPart(this, mdlSwitchOn_,
                {cx, cy + subtleFloat, zBase + glowZAdd},
                w * glowScaleMul,
                h * 0.4f,                  // 細い帯として扱う
                zBase + glowZAdd,
                rotDegZ);
        };


    // ==== S ====
    auto Draw_S = [&](float gx) {
        // 上バー
        DrawBarBase(gx + originX,
            originY + baseH * 0.30f,
            baseW * 0.8f, baseH * 0.18f);

        // 中バー(ちょい右寄り)
        DrawBarBase(gx + originX + baseW * 0.15f,
            originY + 0.0f,
            baseW * 0.7f, baseH * 0.18f);

        // 下バー
        DrawBarBase(gx + originX,
            originY - baseH * 0.30f,
            baseW * 0.8f, baseH * 0.18f);

        // 上左の縦
        DrawBarBase(gx + originX - baseW * 0.30f,
            originY + baseH * 0.15f,
            baseW * 0.18f, baseH * 0.30f);

        // 下右の縦
        DrawBarBase(gx + originX + baseW * 0.30f,
            originY - baseH * 0.15f,
            baseW * 0.18f, baseH * 0.30f);

        // ハイライト帯（上バー上あたり）
        DrawBarGlow(gx + originX,
            originY + baseH * 0.30f,
            baseW * 0.82f, baseH * 0.07f);
        };

    // ==== P ====
    auto Draw_P = [&](float gx) {
        // 縦柱
        DrawBarBase(gx + originX - baseW * 0.30f,
            originY,
            baseW * 0.20f, baseH * 0.80f);

        // 上リング横(上)
        DrawBarBase(gx + originX,
            originY + baseH * 0.20f,
            baseW * 0.70f, baseH * 0.20f);

        // 上リング横(下)
        DrawBarBase(gx + originX,
            originY + baseH * 0.00f,
            baseW * 0.70f, baseH * 0.20f);

        // 上リング右縦
        DrawBarBase(gx + originX + baseW * 0.30f,
            originY + baseH * 0.10f,
            baseW * 0.20f, baseH * 0.40f);

        // ハイライト帯（上リングの上辺）
        DrawBarGlow(gx + originX,
            originY + baseH * 0.20f,
            baseW * 0.50f, baseH * 0.08f);
        };

    // ==== A ====
    auto Draw_A = [&](float gx) {
        // 左脚 (傾け)
        DrawBarBase(gx + originX - baseW * 0.25f,
            originY - baseH * 0.05f,
            baseW * 0.20f, baseH * 0.80f,
            -8.0f);

        // 右脚
        DrawBarBase(gx + originX + baseW * 0.25f,
            originY - baseH * 0.05f,
            baseW * 0.20f, baseH * 0.80f,
            8.0f);

        // 横棒
        DrawBarBase(gx + originX,
            originY + baseH * 0.05f,
            baseW * 0.60f, baseH * 0.18f);

        // ハイライト帯（横棒のあたり）
        DrawBarGlow(gx + originX,
            originY + baseH * 0.05f,
            baseW * 0.62f, baseH * 0.06f);
        };

    // ==== C ====
    auto Draw_C = [&](float gx) {
        // 縦柱
        DrawBarBase(gx + originX - baseW * 0.30f,
            originY,
            baseW * 0.20f, baseH * 0.80f);

        // 上バー
        DrawBarBase(gx + originX,
            originY + baseH * 0.30f,
            baseW * 0.70f, baseH * 0.20f);

        // 下バー
        DrawBarBase(gx + originX,
            originY - baseH * 0.30f,
            baseW * 0.70f, baseH * 0.20f);

        // ハイライト帯（上バー）
        DrawBarGlow(gx + originX,
            originY + baseH * 0.30f,
            baseW * 0.72f, baseH * 0.07f);
        };

    // ==== E ====
    auto Draw_E = [&](float gx) {
        // 縦柱
        DrawBarBase(gx + originX - baseW * 0.30f,
            originY,
            baseW * 0.20f, baseH * 0.80f);

        // 上バー
        DrawBarBase(gx + originX,
            originY + baseH * 0.30f,
            baseW * 0.70f, baseH * 0.20f);

        // 中バー
        DrawBarBase(gx + originX,
            originY + 0.00f,
            baseW * 0.60f, baseH * 0.18f);

        // 下バー
        DrawBarBase(gx + originX,
            originY - baseH * 0.30f,
            baseW * 0.70f, baseH * 0.20f);

        // ハイライト帯（中バー）
        DrawBarGlow(gx + originX,
            originY + 0.00f,
            baseW * 0.62f, baseH * 0.06f);
        };

    // 並べる（S P A C E）
    Draw_S(-2.0f * glyphSpan);
    Draw_P(-1.0f * glyphSpan);
    Draw_A(+0.0f * glyphSpan);
    Draw_C(+1.0f * glyphSpan);
    Draw_E(+2.0f * glyphSpan);
}

// =======================================================
// Draw
// 背景・前景・クレーン・投光器などを並べたあと、最後に "SPACE" サイン
// =======================================================
void TitleScene::Draw() {
    auto *dx = engine_->directXCommon;
    auto *cmd = dx->GetCommandList();
    auto *mr = render_->modelRenderer;

    mr->Begin(cmd, dx, camera_);

    float aspect = std::max(1.0f, (float)dx->GetWidth()) /
        std::max(1.0f, (float)dx->GetHeight());
    float W = virtualWorldH_ * aspect;
    float H = virtualWorldH_;

    auto Lerp = [](float a, float b, float t) { return a + (b - a) * t; };

    // =========================
    // 1) 空：2枚の大板で擬似グラデーション
    // =========================
    DrawModel_(mdlSpring_,
        {0.0f, H * 0.50f, 38.0f},
        {W * 2.6f, H * 2.2f, 0.25f},
        {0,0,0});

    DrawModel_(mdlJumpOnly_,
        {0.0f, H * 0.48f, 37.8f},
        {W * 2.6f, H * 1.9f, 0.25f},
        {0,0,-4.0f});

    // 薄い雲（横長板を数枚）
    for (int i = -3; i <= 3; ++i) {
        float rx = i * (W * 0.35f);
        float ry = H * (0.70f + 0.03f * std::sin(i * 0.9f));
        float rw = W * Lerp(0.50f, 0.80f, Hash01(i * 31));
        float rh = H * 0.06f;
        float rz = 37.6f - (i & 1) * 0.2f;
        DrawModel_(mdlSpring_,
            {rx, ry, rz},
            {rw, rh, 0.05f},
            {0, 0, (i & 1) ? -8.0f : 10.0f});
    }

    // =========================
    // 2) ビル群（遠・中・近）
    // =========================
    auto DrawBuildings = [&](float z, float yBase, float span,
        float wMin, float wMax,
        float hMin, float hMax,
        float tiltDeg, bool warnLight) {
            int count = int(W / span) + 8;
            for (int i = -count / 2; i <= count / 2; ++i) {
                float rx = i * span;
                float r = Hash01(i * 73 + (int)(z * 10));
                float rw = Lerp(wMin, wMax, r);
                float rh = Lerp(hMin, hMax, 1.0f - r);

                // 本体ビル
                DrawModel_(mdlSolid_,
                    {rx, yBase + rh * 0.5f, z},
                    {rw, rh * 0.5f, 0.22f},
                    {0, 0, (i & 1) ? tiltDeg : -tiltDeg});

                // 屋上構造(機材とか箱)
                DrawModel_(mdlSwitchOff_,
                    {rx + rw * 0.14f, yBase + rh + 0.12f, z - 0.03f},
                    {rw * 0.12f, rw * 0.12f, 0.18f},
                    {0,0,(i & 1) ? -6.0f : 8.0f});

                // 警告灯(点いてるやつ)
                if (warnLight && ((i + (int)z) % 5 == 0)) {
                    DrawModel_(mdlSwitchOn_,
                        {rx, yBase + rh + 0.24f, z - 0.05f},
                        {0.10f, 0.10f, 0.15f},
                        {0,0,0});
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

    // =========================
    // 3) クレーン／ワイヤ／吊り荷
    // =========================
    DrawModel_(mdlJumpOnly_,
        {-W * 0.32f, H * 0.86f, 24.8f},
        {0.06f, H * 0.55f, 0.30f},
        {0,0,0});

    DrawModel_(mdlSolid_,
        {-W * 0.06f, H * 1.03f, 24.6f},
        {W * 0.55f, 0.06f, 0.30f},
        {0,0,-9.0f});

    DrawModel_(mdlSolid_,
        {W * 0.22f, H * 0.88f, 24.5f},
        {0.035f,  H * 0.28f, 0.25f},
        {0,0,0});

    DrawModel_(mdlSwitch_,
        {W * 0.22f, H * 0.72f, 24.4f},
        {0.14f,   0.14f,    0.22f},
        {0,0,0});

    // 吊り荷（鉄骨）
    DrawModel_(mdlSolid_,
        {W * 0.22f, H * 0.55f, 24.3f},
        {0.35f, 0.08f, 0.25f},
        {0,0, 4.0f});

    // 作業灯
    DrawModel_(mdlSwitchOn_,
        {W * 0.22f, H * 0.47f, 24.2f},
        {0.15f, 0.08f, 0.22f},
        {0,0, 0.0f});

    // =========================
    // 4) 前景：手すり／補強材／ケーブル
    // =========================
    {
        float railY = -0.6f;

        // 水平の足場
        DrawModel_(mdlSolid_,
            {0.0f, railY, -0.40f},
            {W * 0.66f, 0.05f, 0.22f},
            {0,0,0});

        // 斜めの補強材を等間隔
        for (int i = -3; i <= 3; ++i) {
            float x = i * (W * 0.16f);
            DrawModel_(mdlSolid_,
                {x, railY + 0.20f, -0.41f},
                {W * 0.09f, 0.03f, 0.22f},
                {0, 0, (i % 2 == 0) ? -10.0f : 12.0f});
        }

        // ケーブル（細い板）
        for (int i = -2; i <= 2; ++i) {
            DrawModel_(mdlJumpOnly_,
                {i * (W * 0.18f), railY + 0.55f, -0.42f},
                {W * 0.08f, 0.01f, 0.2f},
                {0,0, 10.0f * std::sinf(float(i))});
        }
    }

    // =========================
    // 5) 投光器（左右）
    // =========================
    auto Flood = [&](XMFLOAT3 b, float rotZ) {
        // 支柱
        DrawModel_(mdlSolid_,
            {b.x, b.y, 22.0f},
            {0.05f, 0.55f, 0.25f},
            {0,0,0});

        // ライトヘッド本体(明るい黄色)
        DrawModel_(mdlSwitchOn_,
            {b.x, b.y + 0.38f, 21.9f},
            {0.22f, 0.12f, 0.22f},
            {0,0, rotZ});

        // 照らしてるコーン(青系: spike)
        DrawModel_(mdlSpike_,
            {b.x + 0.10f, b.y + 0.26f, 21.7f},
            {W * 0.22f, H * 0.10f, 0.05f},
            {0,0, rotZ - 12.0f});

        DrawModel_(mdlSpike_,
            {b.x + 0.08f, b.y + 0.28f, 21.6f},
            {W * 0.24f, H * 0.11f, 0.05f},
            {0,0, rotZ - 14.0f});
        };
    Flood({-W * 0.48f, H * 0.82f, 0}, 10.0f);
    Flood({W * 0.52f, H * 0.74f, 0}, 18.0f);

    // =========================
    // 6) 画面中央ちょい下 "SPACE" サイン
    // =========================
    {
        float glyphW = W * 0.06f;
        float glyphH = H * 0.06f;
        DrawSpaceSign_(glyphW, glyphH);
    }

    mr->End(cmd);
}

// =======================================================
// WIC→SRV
// テクスチャをロードし、指定スロットにSRVを作る
// =======================================================
bool TitleScene::LoadTextureSRV_(
    const std::wstring &fileU16, UINT srvIndex,
    ComPtr<ID3D12Resource> &outTex,
    D3D12_GPU_DESCRIPTOR_HANDLE &outGpuHandle) {
    auto *dx = engine_->directXCommon;
    ID3D12Device *device = dx->GetDevice();
    if (!device || fileU16.empty()) return false;

    DirectX::ScratchImage img, conv;
    HRESULT hr = DirectX::LoadFromWICFile(
        fileU16.c_str(),
        DirectX::WIC_FLAGS_FORCE_SRGB,
        nullptr,
        img
    );
    if (FAILED(hr)) return false;

    const auto &meta = img.GetMetadata();
    DXGI_FORMAT fmt = meta.format;

    // SRGBでなければSRGBに
    if (!DirectX::IsCompressed(meta.format) &&
        !DirectX::IsSRGB(meta.format)) {
        hr = DirectX::Convert(
            img.GetImages(), img.GetImageCount(), meta,
            DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
            DirectX::TEX_FILTER_DEFAULT,
            DirectX::TEX_THRESHOLD_DEFAULT,
            conv
        );
        if (FAILED(hr)) return false;
    }

    const DirectX::Image *src = conv.GetImages() ? conv.GetImages() : img.GetImages();
    auto useMeta = conv.GetMetadata().width ? conv.GetMetadata() : meta;
    fmt = useMeta.format;

    // リソース作成
    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = (UINT)useMeta.width;
    desc.Height = (UINT)useMeta.height;
    desc.DepthOrArraySize = (UINT16)useMeta.arraySize;
    desc.MipLevels = (UINT16)(useMeta.mipLevels ? useMeta.mipLevels : 1);
    desc.Format = fmt;
    desc.SampleDesc = {1,0};
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    hr = device->CreateCommittedResource(
        &heap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&outTex)
    );
    if (FAILED(hr)) return false;

    // アップロードバッファ
    UINT64 uploadSize = GetRequiredIntermediateSize(outTex.Get(), 0, (UINT)useMeta.mipLevels);
    ComPtr<ID3D12Resource> upload = BufferUtility::CreateUploadBuffer(device, uploadSize);

    // one-shot コマンド
    ComPtr<ID3D12CommandQueue> queue;
    {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        device->CreateCommandQueue(&qd, IID_PPV_ARGS(&queue));
    }

    ComPtr<ID3D12CommandAllocator> alloc;
    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc));

    ComPtr<ID3D12GraphicsCommandList> list;
    device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        alloc.Get(), nullptr,
        IID_PPV_ARGS(&list)
    );

    // サブリソース
    std::vector<D3D12_SUBRESOURCE_DATA> subs((size_t)useMeta.mipLevels);
    for (size_t m = 0; m < useMeta.mipLevels; ++m) {
        const DirectX::Image &im = src[m];
        subs[m].pData = im.pixels;
        subs[m].RowPitch = im.rowPitch;
        subs[m].SlicePitch = im.slicePitch;
    }

    UpdateSubresources(
        list.Get(), outTex.Get(), upload.Get(),
        0, 0,
        (UINT)useMeta.mipLevels,
        subs.data()
    );

    auto toSRV = CD3DX12_RESOURCE_BARRIER::Transition(
        outTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
    );
    list->ResourceBarrier(1, &toSRV);
    list->Close();

    ID3D12CommandList *lists[] = {list.Get()};
    queue->ExecuteCommandLists(1, lists);

    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    HANDLE evt = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    queue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1) {
        fence->SetEventOnCompletion(1, evt);
        WaitForSingleObject(evt, INFINITE);
    }
    CloseHandle(evt);

    // === SRVをグローバルヒープの指定スロットに置く ===
    ID3D12DescriptorHeap *heapSrv = dx->GetSrvHeap();
    UINT inc = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE cpu = heapSrv->GetCPUDescriptorHandleForHeapStart();
    cpu.ptr += SIZE_T(inc) * srvIndex;

    D3D12_GPU_DESCRIPTOR_HANDLE gpu = heapSrv->GetGPUDescriptorHandleForHeapStart();
    gpu.ptr += UINT64(inc) * srvIndex;
    outGpuHandle = gpu;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format = fmt;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MostDetailedMip = 0;
    srv.Texture2D.MipLevels = desc.MipLevels;

    device->CreateShaderResourceView(outTex.Get(), &srv, cpu);

    return true;
}
