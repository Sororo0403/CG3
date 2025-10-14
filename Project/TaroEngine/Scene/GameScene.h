#pragma once
#include <optional>
#include <cstdint>
#include "EngineContext.h"
#include "RenderContext.h"
#include "Sprite.h"
#include "TextureManager.h"
#include "IScene.h"

// ▼ 追加
#include "Object3dCommon.h"
#include "Object3d.h"

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext) override;
    void Update(float deltaTime) override;
    void Draw(const EngineContext *engineContext, const RenderContext *renderContext) override;
    void Finalize() override;

private:
    // ==== Sprite UI ====
    float uiX_ = 100.0f, uiY_ = 100.0f;
    float uiW_ = 256.0f, uiH_ = 256.0f;
    float uiCol_[4] = {1, 1, 1, 1};

    Sprite sprite_{};
    TextureManager texMgr_{};
    std::optional<TextureHandle> spriteTex_; // SRV を保持

    // ==== Object3D (plane) ====
    Object3dCommon obj3dCommon_{};       // 3D共通
    Object3d       plane_{};             // 平面モデル
    std::optional<TextureHandle> planeTex_; // 3D用テクスチャ

    // 簡易カメラ
    float camPos_[3] = {0.0f, 4.0f, -8.0f};
    float viewProj_[16]{};               // 行列は列/行優先はシェーダ側に合わせて

    // plane の編集用
    float pPos_[3] = {0.0f, 0.0f, 0.0f};
    float pRot_[3] = {0.0f, 0.0f, 0.0f};       // radians
    float pScl_[3] = {5.0f, 1.0f, 5.0f};
    float pCol_[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};
