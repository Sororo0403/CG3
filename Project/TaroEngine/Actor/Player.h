#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include <string>

class Player {
public:
    /// <summary>
    /// プレイヤーを初期化
    /// </summary>
    /// <param name="device">Direct3Dデバイス</param>
    /// <param name="objPath">読み込むOBJモデルファイルのパス</param>
    void Initialize(ID3D12Device *device, const std::string &objPath);

    /// <summary>
    /// プレイヤーの更新処理
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間(秒)</param>
    void Update(float deltaTime);

    /// <summary>
    /// プレイヤーに関連するリソースを解放
    /// </summary>
    void Finalize();

private:
    // 属性
    Transform transform_;

    // 描画
    Model model_;
};
