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

#include "Texture/TextureDropQueue.h"
#include "WinApp.h"

// Scene
#include "SceneManager.h"
#include "EditorScene.h"

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
    // SceneManager
    // ==================================================
    SceneManager sceneManager;

    sceneManager.ChangeScene(std::make_unique<EditorScene>(
        winApp.get(), dx.get(), input.get(), textureDropQueue.get()));

    // ==================================================
    // Main Loop
    // ==================================================
    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        sceneManager.Update();
        sceneManager.Draw();
    }

    textureDropQueue->Clear();

    return 0;
}
