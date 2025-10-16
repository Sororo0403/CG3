#pragma once
#include <cmath>

/// <summary>
/// 3次元ベクトル（x, y, z）を表す構造体です。
/// </summary>
struct Vector3 {
    float x;
    float y;
    float z;

    /// <summary>
    /// すべての要素を 0.0f に初期化したベクトルを生成して返します。
    /// </summary>
    /// <returns>(0.0f, 0.0f, 0.0f) の Vector3</returns>
    static Vector3 Zero() noexcept { return {0.0f, 0.0f, 0.0f}; }

    /// <summary>
    /// ベクトルの長さ（ノルム）を計算して返します。
    /// </summary>
    /// <returns>√(x² + y² + z²)</returns>
    float Length() const noexcept { return std::sqrt(x * x + y * y + z * z); }

    /// <summary>
    /// ベクトルを正規化した新しいベクトルを返します。
    /// </summary>
    /// <returns>正規化された Vector3（長さ = 1）</returns>
    Vector3 Normalized() const noexcept {
        float len = Length();
        return (len > 0.0f) ? Vector3{x / len, y / len, z / len} : Zero();
    }

    /// <summary>
    /// 2つのベクトルの内積を計算して返します。
    /// </summary>
    /// <param name="a">1つ目のベクトル</param>
    /// <param name="b">2つ目のベクトル</param>
    /// <returns>a・b = a.x * b.x + a.y * b.y + a.z * b.z</returns>
    static float Dot(const Vector3 &a, const Vector3 &b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    /// <summary>
    /// 2つのベクトルの外積を計算して返します。
    /// </summary>
    /// <param name="a">1つ目のベクトル</param>
    /// <param name="b">2つ目のベクトル</param>
    /// <returns>a×b の外積結果（右手系）</returns>
    static Vector3 Cross(const Vector3 &a, const Vector3 &b) noexcept {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    /// <summary>
    /// ベクトル加算。
    /// </summary>
    friend Vector3 operator+(const Vector3 &a, const Vector3 &b) noexcept {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    /// <summary>
    /// ベクトル減算。
    /// </summary>
    friend Vector3 operator-(const Vector3 &a, const Vector3 &b) noexcept {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    /// <summary>
    /// スカラー倍。
    /// </summary>
    friend Vector3 operator*(const Vector3 &v, float s) noexcept {
        return {v.x * s, v.y * s, v.z * s};
    }

    /// <summary>
    /// スカラー除算。
    /// </summary>
    friend Vector3 operator/(const Vector3 &v, float s) noexcept {
        float inv = 1.0f / s;
        return {v.x * inv, v.y * inv, v.z * inv};
    }
};
