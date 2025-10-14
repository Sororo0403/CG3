#pragma once
#include <cmath>
#include "Vector3.h"

namespace VectorUtil {

	/// <summary>
	/// 2つのベクトルのドット積を求めます。
	/// </summary>
	/// <param name="a">ベクトルA。</param>
	/// <param name="b">ベクトルB。</param>
	/// <returns>ドット積。</returns>
	inline float Dot(const Vector3 &a, const Vector3 &b) noexcept {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	/// <summary>
	/// 2つのベクトルのクロス積を求めます。（左手/右手いずれでも実装は同一）
	/// </summary>
	/// <param name="a">ベクトルA。</param>
	/// <param name="b">ベクトルB。</param>
	/// <returns>A × B。</returns>
	inline Vector3 Cross(const Vector3 &a, const Vector3 &b) noexcept {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}

	/// <summary>
	/// ベクトルを正規化します。長さが 0 の場合は (0,0,0) を返します。
	/// </summary>
	/// <param name="v">正規化対象。</param>
	/// <returns>正規化後のベクトル。</returns>
	inline Vector3 Normalize(const Vector3 &v) noexcept {
		const float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
		if (len2 <= 0.0f) return Vector3(0, 0, 0);
		const float inv = 1.0f / std::sqrt(len2);
		return Vector3(v.x * inv, v.y * inv, v.z * inv);
	}
} // namespace VectorUtil
