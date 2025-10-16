#pragma once
#include <cmath>

/// <summary>
/// 4次元ベクトル（x, y, z, w）を表す構造体です。<br/>
/// 3D 変換や射影計算など、同次座標を扱う際に使用します。
/// </summary>
struct Vector4 {
    float x;
    float y;
    float z;
    float w;

    /// <summary>
    /// すべての要素を 0.0f に初期化したベクトルを生成して返します。
    /// </summary>
    /// <returns>(0.0f, 0.0f, 0.0f, 0.0f) の Vector4</returns>
    static Vector4 Zero() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }

    /// <summary>
    /// ベクトルの長さ（ノルム）を計算して返します。
    /// </summary>
    /// <returns>√(x² + y² + z² + w²)</returns>
    float Length() const noexcept { return std::sqrt(x * x + y * y + z * z + w * w); }

    /// <summary>
    /// ベクトルを正規化した新しいベクトルを返します。
    /// </summary>
    /// <returns>正規化された Vector4（長さ = 1）</returns>
    Vector4 Normalized() const noexcept {
        float len = Length();
        return (len > 0.0f) ? Vector4{x / len, y / len, z / len, w / len} : Zero();
    }

    /// <summary>
    /// 2つのベクトルの内積を計算して返します。
    /// </summary>
    /// <param name="a">1つ目のベクトル</param>
    /// <param name="b">2つ目のベクトル</param>
    /// <returns>a・b = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w</returns>
    static float Dot(const Vector4 &a, const Vector4 &b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }

    /// <summary>
    /// ベクトル加算。
    /// </summary>
    friend Vector4 operator+(const Vector4 &a, const Vector4 &b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
    }

    /// <summary>
    /// ベクトル減算。
    /// </summary>
    friend Vector4 operator-(const Vector4 &a, const Vector4 &b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
    }

    /// <summary>
    /// スカラー倍。
    /// </summary>
    friend Vector4 operator*(const Vector4 &v, float s) noexcept {
        return {v.x * s, v.y * s, v.z * s, v.w * s};
    }

    /// <summary>
    /// スカラー除算。
    /// </summary>
    friend Vector4 operator/(const Vector4 &v, float s) noexcept {
        float inv = 1.0f / s;
        return {v.x * inv, v.y * inv, v.z * inv, v.w * inv};
    }
};
