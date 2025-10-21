#pragma once

#include <DirectXMath.h>

struct Transform {
	DirectX::XMFLOAT3 pos = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 rot = {0.0f, 0.0f, 0.0f};
	DirectX::XMFLOAT3 scale = {1.0f, 1.0f, 1.0f};

	/// <summary>
	/// pos, rot, scale からワールド行列を生成。
	/// </summary>
	DirectX::XMMATRIX MakeWorldMatrix() const noexcept {
		DirectX::XMMATRIX s = DirectX::XMMatrixScaling(scale.x, scale.y, scale.z);
		DirectX::XMMATRIX r = DirectX::XMMatrixRotationRollPitchYaw(rot.x, rot.y, rot.z);
		DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(pos.x, pos.y, pos.z);

		return s * r * t;
	}
};
