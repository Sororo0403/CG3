#pragma once

#include <cmath>
#include "Matrix4x4.h"
#include "Vector3.h"
#include "VectorUtil.h"

namespace MatrixUtil {

	/// <summary>
	/// 単位行列を生成します。
	/// </summary>
	/// <returns>単位行列。</returns>
	inline Matrix4x4 MakeIdentityMatrix() {
		Matrix4x4 r{};
		r.m[0][0] = 1; r.m[0][1] = 0; r.m[0][2] = 0; r.m[0][3] = 0;
		r.m[1][0] = 0; r.m[1][1] = 1; r.m[1][2] = 0; r.m[1][3] = 0;
		r.m[2][0] = 0; r.m[2][1] = 0; r.m[2][2] = 1; r.m[2][3] = 0;
		r.m[3][0] = 0; r.m[3][1] = 0; r.m[3][2] = 0; r.m[3][3] = 1;
		return r;
	}

	/// <summary>
	/// スケーリング行列を生成します。
	/// </summary>
	/// <param name="sx">X軸方向のスケール。</param>
	/// <param name="sy">Y軸方向のスケール。</param>
	/// <param name="sz">Z軸方向のスケール。</param>
	/// <returns>スケーリング行列。</returns>
	inline Matrix4x4 MakeScaleMatrix(float sx, float sy, float sz) {
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[0][0] = sx;
		r.m[1][1] = sy;
		r.m[2][2] = sz;
		return r;
	}

	/// <summary>
	/// Z軸回転行列を生成します。
	/// </summary>
	/// <param name="rad">回転角（ラジアン）。</param>
	/// <returns>Z軸回転行列。</returns>
	inline Matrix4x4 MakeRotationZMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[0][0] = c; r.m[0][1] = s;
		r.m[1][0] = -s; r.m[1][1] = c;
		return r;
	}

	/// <summary>
	/// 平行移動行列を生成します。
	/// </summary>
	/// <param name="tx">X方向の移動量。</param>
	/// <param name="ty">Y方向の移動量。</param>
	/// <param name="tz">Z方向の移動量。</param>
	/// <returns>平行移動行列。</returns>
	inline Matrix4x4 MakeTranslationMatrix(float tx, float ty, float tz) {
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[3][0] = tx;
		r.m[3][1] = ty;
		r.m[3][2] = tz;
		return r;
	}

	/// <summary>
	/// 左手座標系の正射影行列を生成します。（DirectXMath::XMMatrixOrthographicLH 相当）
	/// </summary>
	/// <param name="width">投影幅。</param>
	/// <param name="height">投影高さ。</param>
	/// <param name="zn">近クリップ面。</param>
	/// <param name="zf">遠クリップ面。</param>
	/// <returns>正射影行列。</returns>
	inline Matrix4x4 MakeOrthographicMatrix(float width, float height, float zn, float zf) {
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

	/// <summary>
	/// 2つの4x4行列を乗算します。
	/// </summary>
	/// <param name="a">左辺の行列。</param>
	/// <param name="b">右辺の行列。</param>
	/// <returns>a × b の積行列。</returns>
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

	/// <summary>
	/// 行列を転置します。
	/// </summary>
	/// <param name="a">転置対象の行列。</param>
	/// <returns>転置後の行列。</returns>
	inline Matrix4x4 Transpose(const Matrix4x4 &a) {
		Matrix4x4 r{};
		for (int i = 0; i < 4; i++)
			for (int j = 0; j < 4; j++)
				r.m[i][j] = a.m[j][i];
		return r;
	}

	//=====================
	// 回転行列
	//=====================

	/// <summary>
	/// X軸回りの回転行列を生成します。
	/// </summary>
	inline Matrix4x4 MakeRotationXMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[1][1] = c;  r.m[1][2] = s;
		r.m[2][1] = -s; r.m[2][2] = c;
		return r;
	}

	/// <summary>
	/// Y軸回りの回転行列を生成します。
	/// </summary>
	inline Matrix4x4 MakeRotationYMatrix(float rad) {
		Matrix4x4 r = MakeIdentityMatrix();
		float c = std::cos(rad);
		float s = std::sin(rad);
		r.m[0][0] = c; r.m[0][2] = -s;
		r.m[2][0] = s; r.m[2][2] = c;
		return r;
	}

	//=====================
	// ビュー行列（左手系）
	//=====================

	/// <summary>
	/// 左手座標系のビュー行列を生成します。
	/// </summary>
	/// <param name="eye">カメラの位置。</param>
	/// <param name="target">注視点。</param>
	/// <param name="up">上方向ベクトル。</param>
	/// <returns>ビュー行列。</returns>
	inline Matrix4x4 MakeViewMatrix(const Vector3 &eye, const Vector3 &target, const Vector3 &up) {
		Vector3 zaxis = VectorUtil::Normalize(Vector3(target.x - eye.x, target.y - eye.y, target.z - eye.z)); // forward
		Vector3 xaxis = VectorUtil::Normalize(VectorUtil::Cross(up, zaxis));   // right
		Vector3 yaxis = VectorUtil::Cross(zaxis, xaxis);           // up（再直交化）

		Matrix4x4 r{};
		r.m[0][0] = xaxis.x; r.m[0][1] = xaxis.y; r.m[0][2] = xaxis.z; r.m[0][3] = 0.0f;
		r.m[1][0] = yaxis.x; r.m[1][1] = yaxis.y; r.m[1][2] = yaxis.z; r.m[1][3] = 0.0f;
		r.m[2][0] = zaxis.x; r.m[2][1] = zaxis.y; r.m[2][2] = zaxis.z; r.m[2][3] = 0.0f;
		r.m[3][0] = -VectorUtil::Dot(xaxis, eye);
		r.m[3][1] = -VectorUtil::Dot(yaxis, eye);
		r.m[3][2] = -VectorUtil::Dot(zaxis, eye);
		r.m[3][3] = 1.0f;
		return r;
	}

	/// <summary>
	/// 別名：MakeLookAtMatrixLH。左手座標系のビュー行列を生成します。
	/// </summary>
	inline Matrix4x4 MakeLookAtMatrixLH(const Vector3 &eye, const Vector3 &target, const Vector3 &up) {
		return MakeViewMatrix(eye, target, up);
	}

	//=====================
	// 投影行列
	//=====================

	/// <summary>
	/// 左手座標系の透視投影行列を生成します。（DirectXMath::XMMatrixPerspectiveFovLH 相当）
	/// </summary>
	/// <param name="fovY">垂直方向の視野角（ラジアン）。</param>
	/// <param name="aspect">アスペクト比（幅 / 高さ）。</param>
	/// <param name="zn">近クリップ面。</param>
	/// <param name="zf">遠クリップ面。</param>
	/// <returns>透視投影行列。</returns>
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

	/// <summary>
	/// 左手座標系のオフセンター正射影行列を生成します。
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

	//=====================
	// TRS 合成と剛体逆行列
	//=====================

	/// <summary>
	/// スケール・回転・平行移動を合成した TRS 行列を生成します。<br/>
	/// 行メジャー右掛けで、S → RzRyRx → T の順に合成します。
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
	/// 剛体変換（回転＋平行移動）に対する高速な逆行列を求めます。<br/>
	/// 回転は転置し、平行移動は逆回転して反転します。
	/// </summary>
	inline Matrix4x4 InverseRigid(const Matrix4x4 &m) {
		Matrix4x4 r = MakeIdentityMatrix();
		r.m[0][0] = m.m[0][0]; r.m[0][1] = m.m[1][0]; r.m[0][2] = m.m[2][0];
		r.m[1][0] = m.m[0][1]; r.m[1][1] = m.m[1][1]; r.m[1][2] = m.m[2][1];
		r.m[2][0] = m.m[0][2]; r.m[2][1] = m.m[1][2]; r.m[2][2] = m.m[2][2];

		float tx = m.m[3][0], ty = m.m[3][1], tz = m.m[3][2];
		r.m[3][0] = -(tx * r.m[0][0] + ty * r.m[1][0] + tz * r.m[2][0]);
		r.m[3][1] = -(tx * r.m[0][1] + ty * r.m[1][1] + tz * r.m[2][1]);
		r.m[3][2] = -(tx * r.m[0][2] + ty * r.m[1][2] + tz * r.m[2][2]);
		r.m[3][3] = 1.0f;
		return r;
	}

} // namespace MatrixUtil
