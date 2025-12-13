#include <Windows.h>
#include <DirectXMath.h>
#include <memory>
#include <string>

#include "CrashHandler/CrashHandler.h"
#include "DirectX/DirectXCommon.h"
#include "Input.h"

#include "Logger/ConsoleLogger.h"
#include "Logger/FileLogger.h"
#include "Logger/ILogger.h"
#include "Logger/LoggerManager.h"

#include "Shader/ShaderCompiler.h"
#include "Texture/TextureManager.h"
#include "Texture/TextureDropQueue.h"
#include "Texture/TextureEditor.h"

#include "Mesh/MeshManager.h"
#include "Model/ModelManager.h"
#include "Model/ModelRenderer.h"
#include "Model/ModelEditor.h"

#include "Camera/Camera.h"

#include "PSO/PSOManager.h"
#include "Sprite/SpriteRenderer.h"
#include "Sprite/SpriteManager.h"
#include "Sprite/SpriteEditor.h"
#include "Sprite/SpriteLayer.h"

#include "WinApp/WinApp.h"

using namespace DirectX;

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

    // ==================================================
    // CrashHandler
    // ==================================================
    CrashHandler::Install();

    // ==================================================
    // Logger
    // ==================================================
    auto *loggerManager = LoggerManager::GetInstance();
    loggerManager->AddLogger(std::make_unique<ConsoleLogger>());
    loggerManager->AddLogger(std::make_unique<FileLogger>());

#ifdef _DEBUG
    loggerManager->SetMinLevel(LogLevel::DEBUG);
#else
    loggerManager->SetMinLevel(LogLevel::INFO);
#endif
    // ==================================================
    // TextureDropQueue
    // ==================================================
    auto textureDropQueue = std::make_unique<TextureDropQueue>();

    // ==================================================
    // WinApp
    // ==================================================
    constexpr LONG kClientWidth = 1280;
    constexpr LONG kClientHeight = 720;
    const std::wstring kWindowTitle = L"GE3";

    auto winApp = std::make_unique<WinApp>(
        kClientWidth, kClientHeight, kWindowTitle, textureDropQueue.get());
    winApp->Initialize();

    // ==================================================
    // Input
    // ==================================================
    auto input = std::make_unique<Input>();
    input->Initialize(winApp->GetHInstance(), winApp->GetHwnd());

    // ==================================================
    // DirectX
    // ==================================================
    auto dx = std::make_unique<DirectXCommon>(
        winApp->GetHwnd(), winApp->GetWidth(), winApp->GetHeight());
    dx->Initialize();

    // ==================================================
    // ShaderCompiler
    // ==================================================
    auto shaderCompiler = std::make_unique<ShaderCompiler>();
    shaderCompiler->Initialize();
    shaderCompiler->SetShaderRoot("Resources/Shaders");

    // ==================================================
    // TextureManager
    // ==================================================
    auto textureManager = std::make_unique<TextureManager>(dx.get());
    textureManager->Initialize();

    // ==================================================
    // textureEditor (ImGui)
    // ==================================================
    auto textureEditor = std::make_unique<TextureEditor>(
        textureManager.get(), textureDropQueue.get());

    // ==================================================
    // PSOManager
    // ==================================================
    auto psoManager =
        std::make_unique<PSOManager>(dx->GetDevice(), shaderCompiler.get());
    psoManager->Initialize();

    // ==================================================
    // SpriteRenderer
    // ==================================================
    auto spriteRenderer = std::make_unique<SpriteRenderer>(
        dx.get(), shaderCompiler.get(), textureManager.get(), psoManager.get(),
        static_cast<float>(winApp->GetWidth()),
        static_cast<float>(winApp->GetHeight()));
    spriteRenderer->Initialize();

    // ==================================================
    // SpriteManager
    // ==================================================
    auto spriteManager = std::make_unique<SpriteManager>(spriteRenderer.get());

    // ==================================================
    // SpriteEditor (ImGui)
    // ==================================================
    auto spriteEditor = std::make_unique<SpriteEditor>(spriteManager.get(),
                                                       textureEditor.get());
    spriteEditor->Initialize();

    // ==================================================
    // MeshManager
    // ==================================================
    auto meshManager = std::make_unique<MeshManager>(dx->GetDevice());

    // ==================================================
    // ModelRenderer
    // ==================================================
    auto modelRenderer = std::make_unique<ModelRenderer>(
        dx.get(), psoManager.get(), meshManager.get());
    modelRenderer->Initialize();

    // ==================================================
    // ModelManager
    // ==================================================
    auto modelManager =
        std::make_unique<ModelManager>(modelRenderer.get(), meshManager.get());

    // ==================================================
    // ModelEditor (ImGui)
    // ==================================================
    auto modelEditor =
        std::make_unique<ModelEditor>(meshManager.get(), modelManager.get());

    // ==================================================
    // Sprite 作成（テスト用）
    // ==================================================
    uint32_t texId =
        textureManager->LoadTexture("Resources/Textures/uvChecker.png");

    spriteManager->Create(texId, SpriteLayer::UI);

    // ==================================================
    // Camera
    // ==================================================
    Camera camera;
    camera.SetPosition({0.0f, 0.0f, -5.0f});
    camera.SetPerspective(DirectX::XM_PIDIV4,
                          float(winApp->GetWidth()) / winApp->GetHeight(), 0.1f,
                          1000.0f);

    // ==================================================
    // Clear Color
    // ==================================================
    const float kClearColor[4] = {0.1f, 0.1f, 0.2f, 1.0f};

    // ==================================================
    // Main Loop
    // ==================================================
    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        input->Update();

        // ===============================
        // Texture Editor
        // ===============================
        textureEditor->Update();

        dx->PreDraw(kClearColor);

        // ===============================
        // Texture Editor
        // ===============================
        textureEditor->DrawImGui();

        // ===============================
        // Sprite Editor
        // ===============================
        spriteEditor->DrawImGui();

        // ===============================
        // Model Editor
        // ===============================
        modelEditor->DrawImGui();

        // ===============================
        // Model Draw
        // ===============================
        modelManager->Begin();
        modelManager->DrawAll(&camera);

        // ===============================
        // Sprite Draw
        // ===============================
        spriteManager->Begin();
        spriteManager->DrawAll();

        dx->PostDraw();
    }

    return 0;
}
