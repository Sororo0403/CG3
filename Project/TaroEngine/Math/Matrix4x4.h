#pragma once
#include <cmath>

/// <summary>
/// 4x4 の行列を表す構造体。<br/>
/// 各要素は row-major 順で格納され、行ベクトルのポスト乗算（p' = p * M）を想定します。
/// </summary>
struct Matrix4x4 {
    float m[4][4];

    //===============================================================
    // 基本行列生成
    //===============================================================

    /// <summary>
    /// すべての要素が 0.0f の行列を生成して返します。
    /// </summary>
    /// <returns>全要素が 0.0f の Matrix4x4 型の行列</returns>
    static Matrix4x4 Zero() noexcept {
        Matrix4x4 mat{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                mat.m[i][j] = 0.0f;
            }
        }
        return mat;
    }

    /// <summary>
    /// 4x4 の単位行列を生成して返します。
    /// </summary>
    /// <returns>すべての対角要素が 1.0f、その他の要素が 0.0f である Matrix4x4 型の単位行列</returns>
    static Matrix4x4 Identity() noexcept {
        Matrix4x4 mat{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                mat.m[i][j] = (i == j) ? 1.0f : 0.0f;
            }
        }
        return mat;
    }

    //===============================================================
    // 変換行列生成
    //===============================================================

    /// <summary>
    /// X・Y・Z 各軸のスケール値から拡大縮小行列を生成して返します。
    /// </summary>
    /// <param name="sx">X 軸方向のスケール</param>
    /// <param name="sy">Y 軸方向のスケール</param>
    /// <param name="sz">Z 軸方向のスケール</param>
    /// <returns>指定されたスケールを持つ Matrix4x4 型の行列</returns>
    static Matrix4x4 Scale(float sx, float sy, float sz) noexcept {
        Matrix4x4 mat = Identity();
        mat.m[0][0] = sx;
        mat.m[1][1] = sy;
        mat.m[2][2] = sz;
        return mat;
    }

    /// <summary>
    /// Z 軸を中心に回転する行列を生成して返します。
    /// </summary>
    /// <param name="radian">回転角（ラジアン単位）</param>
    /// <returns>Z 軸回転を表す Matrix4x4 型の行列</returns>
    static Matrix4x4 RotationZ(float radian) noexcept {
        Matrix4x4 mat = Identity();
        float c = std::cos(radian);
        float s = std::sin(radian);
        mat.m[0][0] = c;
        mat.m[0][1] = s;
        mat.m[1][0] = -s;
        mat.m[1][1] = c;
        return mat;
    }

    /// <summary>
    /// 平行移動行列を生成して返します。
    /// </summary>
    /// <param name="tx">X 軸方向の移動量</param>
    /// <param name="ty">Y 軸方向の移動量</param>
    /// <param name="tz">Z 軸方向の移動量</param>
    /// <returns>指定された平行移動を表す Matrix4x4 型の行列</returns>
    static Matrix4x4 Translation(float tx, float ty, float tz) noexcept {
        Matrix4x4 mat = Identity();
        mat.m[3][0] = tx;
        mat.m[3][1] = ty;
        mat.m[3][2] = tz;
        return mat;
    }

    //===============================================================
    // 行列演算
    //===============================================================

    /// <summary>
    /// 行列同士を掛け合わせた結果を返します。
    /// </summary>
    /// <param name="lhs">左辺の行列</param>
    /// <param name="rhs">右辺の行列</param>
    /// <returns>lhs と rhs を掛け合わせた結果の Matrix4x4 型の行列</returns>
    friend Matrix4x4 operator*(const Matrix4x4 &lhs, const Matrix4x4 &rhs) noexcept {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    sum += lhs.m[i][k] * rhs.m[k][j];
                }
                result.m[i][j] = sum;
            }
        }
        return result;
    }

    /// <summary>
    /// 行列にスカラー値を掛けた結果を返します。
    /// </summary>
    /// <param name="lhs">行列</param>
    /// <param name="scalar">掛けるスカラー値</param>
    /// <returns>各要素に scalar を掛けた結果の Matrix4x4 型の行列</returns>
    friend Matrix4x4 operator*(const Matrix4x4 &lhs, float scalar) noexcept {
        Matrix4x4 result{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = lhs.m[i][j] * scalar;
            }
        }
        return result;
    }

    /// <summary>
    /// 行列をスカラー値で割った結果を返します。
    /// </summary>
    /// <param name="lhs">行列</param>
    /// <param name="scalar">割るスカラー値</param>
    /// <returns>各要素を scalar で割った結果の Matrix4x4 型の行列</returns>
    friend Matrix4x4 operator/(const Matrix4x4 &lhs, float scalar) noexcept {
        Matrix4x4 result{};
        float inv = 1.0f / scalar;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                result.m[i][j] = lhs.m[i][j] * inv;
            }
        }
        return result;
    }

    //===============================================================
    // ユーティリティ
    //===============================================================

    /// <summary>
    /// 行列を転置した新しい行列を返します。
    /// </summary>
    /// <returns>行と列を入れ替えた Matrix4x4 型の行列</returns>
    Matrix4x4 Transpose() const noexcept {
        Matrix4x4 mat{};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                mat.m[i][j] = m[j][i];
            }
        }
        return mat;
    }

    /// <summary>
    /// 行列の生データ（float[16]）の先頭ポインタを取得します。
    /// </summary>
    /// <returns>この行列の先頭要素のポインタ</returns>
    float *Data() noexcept { return &m[0][0]; }

    /// <summary>
    /// 行列の生データ（float[16]）の先頭ポインタを取得します（読み取り専用）。
    /// </summary>
    /// <returns>この行列の先頭要素の const ポインタ</returns>
    const float *Data() const noexcept { return &m[0][0]; }
};
