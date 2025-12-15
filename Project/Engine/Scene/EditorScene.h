#pragma once
#include "Scene.h"

#include <memory>

// forward declarations
class WinApp;
class Input;
class DirectXCommon;
class ShaderCompiler;
class TextureManager;
class TextureEditor;
class TextureDropQueue;
class PSOManager;
class SpriteRenderer;
class SpriteManager;
class SpriteEditor;
class MeshManager;
class ModelRenderer;
class ModelManager;
class ModelEditor;
class OBJLoader;
class OBJManager;
class Camera;

/// <summary>
/// エディタ用シーン
/// </summary>
class EditorScene : public Scene {
  public:
    EditorScene(WinApp *winApp, DirectXCommon *dx, Input *input,
                TextureDropQueue *textureDropQueue);

    void Initialize() override;
    void Update() override;
    void Draw() override;

  private:
    WinApp *winApp_ = nullptr;
    DirectXCommon *dx_ = nullptr;
    Input *input_ = nullptr;
    TextureDropQueue *textureDropQueue_ = nullptr;

    // === resources / systems ===
    std::unique_ptr<ShaderCompiler> shaderCompiler_;
    std::unique_ptr<TextureManager> textureManager_;
    std::unique_ptr<TextureEditor> textureEditor_;

    std::unique_ptr<PSOManager> psoManager_;
    std::unique_ptr<SpriteRenderer> spriteRenderer_;
    std::unique_ptr<SpriteManager> spriteManager_;
    std::unique_ptr<SpriteEditor> spriteEditor_;

    std::unique_ptr<OBJLoader> objLoader_;
    std::unique_ptr<MeshManager> meshManager_;
    std::unique_ptr<ModelRenderer> modelRenderer_;
    std::unique_ptr<ModelManager> modelManager_;
    std::unique_ptr<OBJManager> objManager_;
    std::unique_ptr<ModelEditor> modelEditor_;

    std::unique_ptr<Camera> camera_;
};
