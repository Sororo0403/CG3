#pragma once

#include <memory>
#include <string>

#include "Mesh/Mesh.h"

/// <summary>
/// Wavefront OBJ ローダー（最小構成）
/// v / vt / vn / f に対応
/// </summary>
class OBJLoader {
  public:
    /// <summary>
    /// デストラクタ
    /// </summary>
    ~OBJLoader() = default;

    /// <summary>
    /// OBJ ファイルを読み込み、Mesh を生成する
    /// </summary>
    /// <param name="path">OBJ ファイルパス</param>
    /// <returns>Mesh（CPU 側）</returns>
    std::unique_ptr<Mesh> Load(const std::string &path);
};
