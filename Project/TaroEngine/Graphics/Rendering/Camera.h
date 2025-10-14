#pragma once

#include "Matrix4x4.h"
#include "Vector3.h"
#include <numbers>

class Camera {
public:
	/// <summary>
	/// コンストラクタ。
	/// </summary>
	Camera() = default;

	/// <summary>
	/// デストラクタ。
	/// </summary>
	~Camera() = default;

	/// <summary>
	/// 初期化。
	/// </summary>
	/// <param name="viewportWidth">ビューポート幅</param>
	/// <param name="viewportHeight">ビューポート高さ</param>
	/// <param name="fovYRadians">垂直 FOV（ラジアン）</param>
	/// <param name="nearZ">ニア平面</param>
	/// <param name="farZ">ファー平面</param>
	void Initialize(
		float viewportWidth,
		float viewportHeight,
		float fovYRadians = 60.0f * std::numbers::pi_v<float> / 180.0f,
		float nearZ = 0.1f,
		float farZ = 1000.0f);

	/// <summary>
	/// 毎フレーム更新。dirty のとき行列を再計算する。
	/// </summary>
	void Update();

private:
	// パラメータ
	Vector3 position_{0.0f, 0.0f, -5.0f};
	Vector3 target_{0.0f, 0.0f,  0.0f};
	Vector3 up_{0.0f, 1.0f,  0.0f};

	float fovY_ = 60.0f * std::numbers::pi_v<float> / 180.0f;
	float aspect_ = 16.0f / 9.0f;
	float nearZ_ = 0.1f;
	float farZ_ = 1000.0f;

	// 行列
	Matrix4x4 view_;
	Matrix4x4 proj_;
	Matrix4x4 viewProj_;

	bool dirty_ = true;

	/// <summary>
	/// 内部計算：View/Proj/ViewProj を再生成。
	/// </summary>
	void Recalculate_();

	/// <summary>
	/// Yaw/Pitch から forward を作って target を更新。
	/// </summary>
	void ApplyYawPitch_(float yaw, float pitch);
};
