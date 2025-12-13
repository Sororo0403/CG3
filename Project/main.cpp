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
#include "Sprite/SpriteRenderer.h"
#include "Sprite/SpriteManager.h"
#include "Texture/TextureManager.h"
#include "WinApp/WinApp.h"

using namespace DirectX;

// ===============================
// ImGui 用デバッグ変数
// ===============================
static float spritePos[2] = {100.0f, 100.0f};
static float spriteSize[2] = {256.0f, 256.0f};
static float spriteColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};

static float spriteRotDeg = 0.0f;
static float spriteZ = 0.0f;
static float spritePivot[2] = {0.5f, 0.5f};
static float spriteUV[4] = {0.0f, 0.0f, 1.0f, 1.0f};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

  // ==================================================
  // CrashHandler
  // ==================================================
  CrashHandler::Install();

  // ==================================================
  // Logger
  // ==================================================
  LoggerManager *loggerManager = LoggerManager::GetInstance();
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
  const LONG kClientWidth = 1280;
  const LONG kClientHeight = 720;
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

  // 正射影行列は Renderer に一度だけ渡す
  spriteRenderer->SetProjection(XMMatrixOrthographicOffCenterLH(
      0.0f, static_cast<float>(winApp->GetWidth()),
      static_cast<float>(winApp->GetHeight()), 0.0f, 0.0f, 1.0f));

  // ==================================================
  // SpriteManager
  // ==================================================
  auto spriteManager = std::make_unique<SpriteManager>(spriteRenderer.get());

  // ==================================================
  // Sprite 作成（純データ）
  // ==================================================
  uint32_t texId =
      textureManager->LoadTexture("Resources/Textures/uvChecker.png");

  spriteManager->Create(texId, SpriteManager::Layer::UI);

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
    // Sprite 描画
    // ===============================
    spriteRenderer->Begin();
    spriteManager->Begin();
    spriteManager->DrawAll();

    dx->PostDraw();
  }

  return 0;
}
