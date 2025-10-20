#define NOMINMAX
#include "GameScene.h"
#include "DirectXCommon.h"
#include <DirectXMath.h>

using namespace DirectX;

void GameScene::Initialize(const EngineContext *engineContext, const RenderContext *renderContext) {
    engineContext_ = engineContext;
    renderContext_ = renderContext;

    auto *dx = engineContext_->directXCommon;

    // PSO（シェーダは ModelRenderer 内でコンパイル）
    renderer_.Initialize(dx->GetDevice(), renderContext_->shaderCompiler);

    // モデル読み込み（相対パスはワーキングディレクトリに依存）
    player_.Initialize(dx->GetDevice(), "Resources/Model/Player/player.obj");

    // カメラ：原点(0,0,0) を見つつ、原点から少し手前（-Z）に配置
    // 目安：Blender既定スケール(1=1m)で少し引き、軽く見下ろさない設定
    camera_.Reset();
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());
    camera_.SetEye({0.0f,  1.2f, -10.0f});   // ← 原点から“少し手前”
    camera_.SetTarget({0.0f,  1.2f,  0.0f});
    camera_.SetUp({0.0f, 1.0f, 0.0f});
    camera_.SetPerspective(60.0f, camera_.GetAspect(), 0.1f, 1000.0f);
}

void GameScene::Draw() {
    auto *dx = engineContext_->directXCommon;
    auto *cmd = dx->GetCommandList();

    // 画面サイズが変わっても安全
    camera_.SetViewportSize(dx->GetWidth(), dx->GetHeight());

    // 転置済み VP を用意（ModelRenderer の HLSL 仕様に合わせる）
    float vT[16], pT[16];
    camera_.GetTransposeVP(vT, pT);

    // 単位ワールド（player.obj をそのままのスケールで）
    float wT[16];
    {
        XMMATRIX W = XMMatrixIdentity();
        Camera::StoreT(wT, W); // 転置した 4x4 を書き出し
    }

    // ベタ色（白）
    constexpr float kWhite[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    renderer_.Begin(cmd, vT, pT);
    renderer_.Draw(cmd, player_.GetMesh(), wT, kWhite);
    renderer_.End(cmd);
}

void GameScene::Finalize() {
    renderer_.Finalize();
}
