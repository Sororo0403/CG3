#include "EditorScene.h"

#include "WinApp/WinApp.h"
#include "DirectX/DirectXCommon.h"
#include "Input.h"

#include "Shader/ShaderCompiler.h"
#include "Texture/TextureManager.h"
#include "Texture/TextureEditor.h"
#include "Texture/TextureDropQueue.h"

#include "PSO/PSOManager.h"

#include "Sprite/SpriteRenderer.h"
#include "Sprite/SpriteManager.h"
#include "Sprite/SpriteEditor.h"
#include "Sprite/SpriteLayer.h"

#include "OBJ/OBJLoader.h"
#include "OBJ/OBJManager.h"

#include "Mesh/MeshManager.h"
#include "Model/ModelRenderer.h"
#include "Model/ModelManager.h"
#include "Model/ModelEditor.h"

#include "Camera/Camera.h"

EditorScene::EditorScene(WinApp *winApp, DirectXCommon *dx, Input *input)
    : winApp_(winApp), dx_(dx), input_(input) {
}

void EditorScene::Initialize() {
    textureDropQueue_ = std::make_unique<TextureDropQueue>();

    shaderCompiler_ = std::make_unique<ShaderCompiler>();
    shaderCompiler_->Initialize();
    shaderCompiler_->SetShaderRoot("Resources/Shaders");

    textureManager_ = std::make_unique<TextureManager>(dx_);
    textureManager_->Initialize();

    textureEditor_ = std::make_unique<TextureEditor>(textureManager_.get(),
                                                     textureDropQueue_.get());

    psoManager_ =
        std::make_unique<PSOManager>(dx_->GetDevice(), shaderCompiler_.get());
    psoManager_->Initialize();

    spriteRenderer_ = std::make_unique<SpriteRenderer>(
        dx_, shaderCompiler_.get(), textureManager_.get(), psoManager_.get(),
        float(winApp_->GetWidth()), float(winApp_->GetHeight()));
    spriteRenderer_->Initialize();

    spriteManager_ = std::make_unique<SpriteManager>(spriteRenderer_.get());
    spriteEditor_ = std::make_unique<SpriteEditor>(spriteManager_.get(),
                                                   textureEditor_.get());
    spriteEditor_->Initialize();

    objLoader_ = std::make_unique<OBJLoader>();
    meshManager_ =
        std::make_unique<MeshManager>(dx_->GetDevice(), objLoader_.get());

    modelRenderer_ = std::make_unique<ModelRenderer>(dx_, psoManager_.get(),
                                                     meshManager_.get());
    modelRenderer_->Initialize();

    modelManager_ = std::make_unique<ModelManager>(modelRenderer_.get(),
                                                   meshManager_.get());

    objManager_ =
        std::make_unique<OBJManager>(meshManager_.get(), modelManager_.get());

    modelEditor_ = std::make_unique<ModelEditor>(
        meshManager_.get(), modelManager_.get(), objManager_.get());

    // テスト用 Sprite
    uint32_t texId =
        textureManager_->LoadTexture("Resources/Textures/uvChecker.png");
    spriteManager_->Create(texId, SpriteLayer::UI);

    camera_ = std::make_unique<Camera>();
    camera_->SetPosition({0.0f, 0.0f, -5.0f});
    camera_->SetPerspective(DirectX::XM_PIDIV4,
                            float(winApp_->GetWidth()) / winApp_->GetHeight(),
                            0.1f, 1000.0f);
}

void EditorScene::Update() {
    input_->Update();
    textureEditor_->Update();
}

void EditorScene::Draw() {
    const float clearColor[4] = {0.1f, 0.1f, 0.2f, 1.0f};
    dx_->PreDraw(clearColor);

    textureEditor_->DrawImGui();
    spriteEditor_->DrawImGui();
    modelEditor_->DrawImGui();

    modelManager_->Begin();
    modelManager_->DrawAll(camera_.get());

    spriteManager_->Begin();
    spriteManager_->DrawAll();

    dx_->PostDraw();
}
