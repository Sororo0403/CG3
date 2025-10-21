#pragma once

#include "Model.h"
#include "ModelRenderer.h"
#include "Transform.h"
#include <string>
#include <DirectXMath.h>

class Input;

class Player {
public:
	/// <summary>
	/// プレイヤーを初期化
	/// </summary>
	/// <param name="device">Direct3Dデバイス</param>
	/// <param name="objPath">読み込むOBJモデルファイルのパス</param>
	/// <param name="input">入力クラス</param>
	void Initialize(ID3D12Device *device, const std::string &objPath, const Input *input);

	/// <summary>
	/// プレイヤーの更新処理
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間(秒)</param>
	void Update(float deltaTime);

	/// <summary>
	/// プレイヤーに関連するリソースを解放
	/// </summary>
	void Finalize();

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

private:
	/// <summary>
	/// 移動処理
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間(秒)</param>
	void Move_(float deltaTime);

private:
	// 属性
	Transform transform_ = {};
	DirectX::XMFLOAT3 velocity_ = {0.0f, 0.0f, 0.0f};
	float speed_ = 1.0f;

	// 描画
	Model model_;

	// 入力
	const Input *input_ = nullptr;
};
