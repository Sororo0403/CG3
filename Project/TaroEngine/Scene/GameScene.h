// GameScene.h
#pragma once
#include "EngineContext.h"
#include "RenderContext.h"
#include "ModelRenderer.h"
#include "Mesh.h"
#include "IScene.h"
#include "Camera.h"
#include <DirectXMath.h>

class GameScene : public IScene {
public:
    void Initialize(const EngineContext *engineContext, const RenderContext *renderContext);
    void Update(float deltaTime);
    void Draw();
    void Finalize();

private:
    ModelRenderer model_;
    Mesh          cube_;
    Camera        camera_;

    DirectX::XMMATRIX world_ = DirectX::XMMatrixIdentity();

    // === UI ===
    bool  autoSpin_ = true;
    float rotSpeed_ = 1.0f;   // [rad/sec]
    float color_[4] = {1,1,1,1};
    float angleY_ = 0.0f;
};
