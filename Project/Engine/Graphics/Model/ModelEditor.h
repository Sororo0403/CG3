#pragma once

#include <cstdint>
#include <string>

class MeshManager;
class ModelManager;
class OBJManager;

/// <summary>
/// Model 編集用（Editor / UI）
/// </summary>
class ModelEditor {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    ModelEditor(MeshManager *meshManager, ModelManager *modelManager,
                OBJManager *objManager);

    /// <summary>
    /// ImGui を描画
    /// </summary>
    void DrawImGui();

  private:
    // Create
    void CreateCubeModel();
    void CreateObjModel(const std::string &path);

  private:
    MeshManager *meshManager_ = nullptr;
    ModelManager *modelManager_ = nullptr;
    OBJManager *objManager_ = nullptr;

    uint32_t currentModelId_ = 0;

    // UI用
    char objPath_[256] = "Resources/Models/Bunny/bunny.obj";
};
