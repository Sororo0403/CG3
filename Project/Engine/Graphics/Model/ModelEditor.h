#pragma once

#include <cstdint>

class MeshManager;
class ModelManager;

/// <summary>
/// Model 編集用（最小）
/// </summary>
class ModelEditor {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    ModelEditor(MeshManager *meshManager, ModelManager *modelManager);

    /// <summary>
    /// ImGuiを描画
    /// </summary>
    void DrawImGui();

  private:
    // Create
    void CreateCubeModel();

  private:
    MeshManager *meshManager_ = nullptr;
    ModelManager *modelManager_ = nullptr;

    uint32_t currentModelId_ = 0;
};
