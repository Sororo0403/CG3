#pragma once

#include "IBlock.h"

class SolidBlock :public IBlock {
	public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="device">Direct3Dデバイス</param>
	/// <param name="objPath">読み込むOBJモデルファイルのパス</param>
	void Initialize(ID3D12Device *device, const std::string &objPath) override;

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">前フレームからの経過時間(秒)</param>
	void Update(float /*deltaTime*/) override;

	/// <summary>
	/// 解放
	/// </summary>
	void Finalize() override;
};

