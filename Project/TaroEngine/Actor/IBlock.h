#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include <string>

class IBlock {
public:
	/// <summary>
	/// プレイヤーを初期化
	/// </summary>
	/// <param name="device">Direct3Dデバイス</param>
	/// <param name="objPath">読み込むOBJモデルファイルのパス</param>
	virtual void Initialize(ID3D12Device *device, const std::string &objPath) = 0;

	/// <summary>
	/// プレイヤーの更新処理
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間(秒)</param>
	virtual void Update(float deltaTime) = 0;

	/// <summary>
	/// プレイヤーに関連するリソースを解放
	/// </summary>
	virtual void Finalize() = 0;

	/// <summary>
	/// モデルを取得
	/// </summary>
	/// <returns>モデル</returns>
	const Model &GetModel() const noexcept { return model_; }

	/// <summary>
	/// トランスフォームを取得
	/// </summary>
	/// <returns>トランスフォーム</returns>
	const Transform &GetTransform() const noexcept { return transform_; }

protected:
	// 属性
	Transform transform_;

	// 描画
	Model model_;
};
