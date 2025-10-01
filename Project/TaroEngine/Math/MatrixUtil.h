#pragma once

#include <cmath>
#include "Matrix4x4.h"
#include "Vector3.h"

namespace MatrixUtil {

	inline Matrix4x4 MakeIdentityMatrix() {
		Matrix4x4 r{};
		r.m[0][0] = 1; r.m[0][1] = 0; r.m[0][2] = 0; r.m[0][3] = 0;
		r.m[1][0] = 0; r.m[1][1] = 1; r.m[1][2] = 0; r.m[1][3] = 0;
		r.m[2][0] = 0; r.m[2][1] = 0; r.m[2][2] = 1; r.m[2][3] = 0;
		r.m[3][0] = 0; r.m[3][1] = 0; r.m[3][2] = 0; r.m[3][3] = 1;
		return r;
	}

	inline Matrix4x4 MakeScaleMatrix(float sx, float sy, float sz) {
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[0][0] = sx;
		r.m[1][1] = sy;
		r.m[2][2] = sz;
		return r;
	}

	inline Matrix4x4 MakeRotationZMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[0][0] = c; r.m[0][1] = s;
		r.m[1][0] = -s; r.m[1][1] = c;
		return r;
	}

	inline Matrix4x4 MakeTranslationMatrix(float tx, float ty, float tz) {
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[3][0] = tx;
		r.m[3][1] = ty;
		r.m[3][2] = tz;
		return r;
	}

	// 左手系の正射影 (DirectXMath::XMMatrixOrthographicLH 相当)
	inline Matrix4x4 MakeOrthographicMatrix(float width, float height,
		float zn, float zf) {
		Matrix4x4 r{};
		float rw = 2.0f / width;
		float rh = 2.0f / height;
		float rz = 1.0f / (zf - zn);

		r.m[0][0] = rw; r.m[0][1] = 0;  r.m[0][2] = 0;        r.m[0][3] = 0;
		r.m[1][0] = 0;  r.m[1][1] = rh; r.m[1][2] = 0;        r.m[1][3] = 0;
		r.m[2][0] = 0;  r.m[2][1] = 0;  r.m[2][2] = rz;       r.m[2][3] = 0;
		r.m[3][0] = 0;  r.m[3][1] = 0;  r.m[3][2] = -zn * rz; r.m[3][3] = 1;
		return r;
	}

	inline Matrix4x4 Multiply(const Matrix4x4 &a, const Matrix4x4 &b) {
		Matrix4x4 r{};
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				r.m[i][j] = a.m[i][0] * b.m[0][j] +
					a.m[i][1] * b.m[1][j] +
					a.m[i][2] * b.m[2][j] +
					a.m[i][3] * b.m[3][j];
			}
		}
		return r;
	}

	inline Matrix4x4 Transpose(const Matrix4x4 &a) {
		Matrix4x4 r{};
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = a.m[j][i];
		return r;
	}
	// ---------- ベクトルユーティリティ ----------
	/// <summary>ドット積。</summary>
	inline float Dot(const Vector3 &a, const Vector3 &b) {
		return a.x * b.x + a.y * b.y + a.z * b.z;
	}

	/// <summary>クロス積（左手系でも実装は同じ）。</summary>
	inline Vector3 Cross(const Vector3 &a, const Vector3 &b) {
		return Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x
		);
	}

	/// <summary>正規化。長さ0なら (0,0,0)。</summary>
	inline Vector3 Normalize(const Vector3 &v) {
		float len2 = v.x * v.x + v.y * v.y + v.z * v.z;
		if (len2 <= 0.0f) return Vector3(0, 0, 0);
		float inv = 1.0f / std::sqrt(len2);
		return Vector3(v.x * inv, v.y * inv, v.z * inv);
	}

	// ---------- 回転行列（X / Y / Z） ----------
	/// <summary>X軸回り回転行列（ラジアン）。</summary>
	inline Matrix4x4 MakeRotationXMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[1][1] = c;  r.m[1][2] = s;
		r.m[2][1] = -s; r.m[2][2] = c;
		return r;
	}

	/// <summary>Y軸回り回転行列（ラジアン）。</summary>
	inline Matrix4x4 MakeRotationYMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[0][0] = c; r.m[0][2] = -s;
		r.m[2][0] = s; r.m[2][2] = c;
		return r;
	}

	// 既存の Z 回転はそのまま使用（MakeRotationZMatrix）

	// ---------- ビュー行列（左手・LookAt） ----------
	/// <summary>
	/// 左手座標系のビュー行列。<br/>
	/// forward = normalize(target - eye) を +Z 方向とみなす。
	/// </summary>
	inline Matrix4x4 MakeViewMatrix(const Vector3 &eye, const Vector3 &target, const Vector3 &up) {
		Vector3 zaxis = Normalize(Vector3(target.x - eye.x, target.y - eye.y, target.z - eye.z)); // forward
		Vector3 xaxis = Normalize(Cross(up, zaxis));   // right
		Vector3 yaxis = Cross(zaxis, xaxis);           // up（再直交化）

		Matrix4x4 r{};
		// 行ベクトル（行メジャー）で配置
		r.m[0][0] = xaxis.x; r.m[0][1] = xaxis.y; r.m[0][2] = xaxis.z; r.m[0][3] = 0.0f;
		r.m[1][0] = yaxis.x; r.m[1][1] = yaxis.y; r.m[1][2] = yaxis.z; r.m[1][3] = 0.0f;
		r.m[2][0] = zaxis.x; r.m[2][1] = zaxis.y; r.m[2][2] = zaxis.z; r.m[2][3] = 0.0f;
		r.m[3][0] = -Dot(xaxis, eye);
		r.m[3][1] = -Dot(yaxis, eye);
		r.m[3][2] = -Dot(zaxis, eye);
		r.m[3][3] = 1.0f;
		return r;
	}

	/// <summary>別名：MakeLookAtLH（利き手は左手）。</summary>
	inline Matrix4x4 MakeLookAtMatrixLH(const Vector3 &eye, const Vector3 &target, const Vector3 &up) {
		return MakeViewMatrix(eye, target, up);
	}

	// ---------- 透視投影（左手・FOV指定） ----------
	/// <summary>
	/// 左手座標系の透視投影（DirectXMath::XMMatrixPerspectiveFovLH 相当）。<br/>
	/// fovY: 垂直FOV（ラジアン）, aspect: 幅/高さ, zn/ zf: 近/遠。
	/// </summary>
	inline Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspect, float zn, float zf) {
		float yScale = 1.0f / std::tan(fovY * 0.5f);
		float xScale = yScale / aspect;
		float zRange = zf - zn;

		Matrix4x4 r{};
		r.m[0][0] = xScale; r.m[0][1] = 0.0f;   r.m[0][2] = 0.0f;        r.m[0][3] = 0.0f;
		r.m[1][0] = 0.0f;   r.m[1][1] = yScale; r.m[1][2] = 0.0f;        r.m[1][3] = 0.0f;
		r.m[2][0] = 0.0f;   r.m[2][1] = 0.0f;   r.m[2][2] = zf / zRange; r.m[2][3] = 1.0f;
		r.m[3][0] = 0.0f;   r.m[3][1] = 0.0f;   r.m[3][2] = (-zn * zf) / zRange; r.m[3][3] = 0.0f;
		return r;
	}

	// ---------- 正射影（オフセンター） ----------
	/// <summary>
	/// 左手・オフセンター正射影。<br/>
	/// left/right/top/bottom で投影範囲を直接指定する版。
	/// </summary>
	inline Matrix4x4 MakeOrthographicOffCenterMatrix(float left, float right, float bottom, float top, float zn, float zf) {
		float rl = right - left;
		float tb = top - bottom;
		float fn = zf - zn;

		Matrix4x4 r{};
		r.m[0][0] = 2.0f / rl; r.m[0][1] = 0.0f;       r.m[0][2] = 0.0f;        r.m[0][3] = 0.0f;
		r.m[1][0] = 0.0f;       r.m[1][1] = 2.0f / tb; r.m[1][2] = 0.0f;        r.m[1][3] = 0.0f;
		r.m[2][0] = 0.0f;       r.m[2][1] = 0.0f;       r.m[2][2] = 1.0f / fn;   r.m[2][3] = 0.0f;
		r.m[3][0] = -(left + right) / rl;
		r.m[3][1] = -(bottom + top) / tb;
		r.m[3][2] = -zn / fn;
		r.m[3][3] = 1.0f;
		return r;
	}

	// ---------- TRS 合成と剛体逆行列 ----------
	/// <summary>
	/// 行メジャー×右掛けの TRS 行列（S → RzRyRx → T の順で合成）。<br/>
	/// スケール→回転→平行移動で作る一般的なワールド行列。
	/// </summary>
	inline Matrix4x4 MakeTRS(const Vector3 &t, const Vector3 &rXYZRadians, const Vector3 &s) {
		auto S = MakeScaleMatrix(s.x, s.y, s.z);
		auto Rx = MakeRotationXMatrix(rXYZRadians.x);
		auto Ry = MakeRotationYMatrix(rXYZRadians.y);
		auto Rz = MakeRotationZMatrix(rXYZRadians.z);
		auto R = Multiply(Multiply(Rx, Ry), Rz);
		auto T = MakeTranslationMatrix(t.x, t.y, t.z);
		return Multiply(Multiply(S, R), T);
	}

	/// <summary>
	/// 剛体変換（回転が正規直交・スケール1）向けの高速逆行列。<br/>
	/// R と T だけの行列に対し、R^T と -T を用いて逆を構成する。
	/// </summary>
	inline Matrix4x4 InverseRigid(const Matrix4x4 &m) {
		// 回転部分（上3x3）を転置
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[0][0] = m.m[0][0]; r.m[0][1] = m.m[1][0]; r.m[0][2] = m.m[2][0];
		r.m[1][0] = m.m[0][1]; r.m[1][1] = m.m[1][1]; r.m[1][2] = m.m[2][1];
		r.m[2][0] = m.m[0][2]; r.m[2][1] = m.m[1][2]; r.m[2][2] = m.m[2][2];

		// 平行移動を逆回転して符号反転
		float tx = m.m[3][0], ty = m.m[3][1], tz = m.m[3][2];
		r.m[3][0] = -(tx * r.m[0][0] + ty * r.m[1][0] + tz * r.m[2][0]);
		r.m[3][1] = -(tx * r.m[0][1] + ty * r.m[1][1] + tz * r.m[2][1]);
		r.m[3][2] = -(tx * r.m[0][2] + ty * r.m[1][2] + tz * r.m[2][2]);
		r.m[3][3] = 1.0f;
		return r;
	}

} // namespace MatrixUtil