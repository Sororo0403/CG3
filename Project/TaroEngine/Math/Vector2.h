#pragma once
#include <cmath>

/// <summary>
/// 2次元ベクトル（x, y）を表す構造体です。
/// </summary>
struct Vector2 {
    float x;
    float y;

    /// <summary>
    /// すべての要素を 0.0f に初期化したベクトルを生成して返します。
    /// </summary>
    /// <returns>(0.0f, 0.0f) の Vector2</returns>
    static Vector2 Zero() noexcept { return {0.0f, 0.0f}; }

    /// <summary>
    /// ベクトルの長さ（ノルム）を計算して返します。
    /// </summary>
    /// <returns>√(x² + y²)</returns>
    float Length() const noexcept { return std::sqrt(x * x + y * y); }

    /// <summary>
    /// ベクトルを正規化した新しいベクトルを返します。
    /// </summary>
    /// <returns>正規化された Vector2（長さ = 1）</returns>
    Vector2 Normalized() const noexcept {
        float len = Length();
        return (len > 0.0f) ? Vector2{x / len, y / len} : Zero();
    }

    /// <summary>
    /// 2つのベクトルの内積を計算して返します。
    /// </summary>
    /// <param name="a">1つ目のベクトル</param>
    /// <param name="b">2つ目のベクトル</param>
    /// <returns>a・b = a.x * b.x + a.y * b.y</returns>
    static float Dot(const Vector2 &a, const Vector2 &b) noexcept {
        return a.x * b.x + a.y * b.y;
    }

    /// <summary>
    /// ベクトル加算。
    /// </summary>
    friend Vector2 operator+(const Vector2 &a, const Vector2 &b) noexcept {
        return {a.x + b.x, a.y + b.y};
    }

    /// <summary>
    /// ベクトル減算。
    /// </summary>
    friend Vector2 operator-(const Vector2 &a, const Vector2 &b) noexcept {
        return {a.x - b.x, a.y - b.y};
    }

    /// <summary>
    /// スカラー倍。
    /// </summary>
    friend Vector2 operator*(const Vector2 &v, float s) noexcept {
        return {v.x * s, v.y * s};
    }

    /// <summary>
    /// スカラー除算。
    /// </summary>
    friend Vector2 operator/(const Vector2 &v, float s) noexcept {
        float inv = 1.0f / s;
        return {v.x * inv, v.y * inv};
    }
};
