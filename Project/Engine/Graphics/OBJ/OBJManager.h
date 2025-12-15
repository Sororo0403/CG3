#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

#include "Vector3.h"

class MeshManager;
class ModelManager;
struct Model;

/// <summary>
/// OBJ を簡単に Model として生成するための窓口クラス
/// </summary>
class OBJManager {
  public:
    /// <summary>
    /// コンストラクタ
    /// </summary>
    OBJManager(MeshManager *meshManager, ModelManager *modelManager);

    /// <summary>
    /// OBJ から Model を生成する
    /// （同じ OBJ は Mesh を共有）
    /// </summary>
    uint32_t CreateModel(const std::string &path);

    /// <summary>
    /// OBJ から Model を生成し、Transform を設定する
    /// </summary>
    uint32_t CreateModel(const std::string &path, const Vector3 &position,
                         const Vector3 &rotation = {0, 0, 0},
                         const Vector3 &scale = {1, 1, 1});

  private:
    MeshManager *meshManager_ = nullptr;
    ModelManager *modelManager_ = nullptr;

    // OBJ パス → MeshID キャッシュ
    std::unordered_map<std::string, uint32_t> meshCache_;
};
