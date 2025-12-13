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
  // WinApp
  // ==================================================
  constexpr LONG kClientWidth = 1280;
  constexpr LONG kClientHeight = 720;
  const std::wstring kWindowTitle = L"GE3";

  auto winApp = std::make_unique<WinApp>();
  winApp->Initialize(kClientWidth, kClientHeight, kWindowTitle);

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
  // SpriteRenderer
  // ==================================================
  auto spriteRenderer = std::make_unique<SpriteRenderer>(
      dx.get(), shaderCompiler.get(), textureManager.get());
  spriteRenderer->Initialize();

  // 正射影
  spriteRenderer->SetProjection(XMMatrixOrthographicOffCenterLH(
      0.0f, static_cast<float>(winApp->GetWidth()),
      static_cast<float>(winApp->GetHeight()), 0.0f, 0.0f, 1.0f));

  // ==================================================
  // SpriteManager
  // ==================================================
  auto spriteManager = std::make_unique<SpriteManager>(spriteRenderer.get());

  // ==================================================
  // SpriteEditor (ImGui)
  // ==================================================
  auto spriteEditor = std::make_unique<SpriteEditor>(spriteManager.get());
  spriteEditor->Initialize();

  // ==================================================
  // Sprite 作成（テスト用）
  // ==================================================
  uint32_t texId =
      textureManager->LoadTexture("Resources/Textures/uvChecker.png");

  spriteManager->Create(texId, SpriteLayer::UI);

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

    dx->PreDraw(kClearColor);

    // ===============================
    // Sprite Editor
    // ===============================
    spriteEditor->DrawImGui();

    // ===============================
    // Sprite Draw
    // ===============================
    spriteManager->Begin();
    spriteManager->DrawAll();

    dx->PostDraw();
  }

  return 0;
}
