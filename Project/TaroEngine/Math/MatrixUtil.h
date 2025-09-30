#pragma once

#include <cmath>
#include "Matrix4x4.h"

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

} // namespace MatrixUtil