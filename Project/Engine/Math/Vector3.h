#pragma once

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vector3() = default;
    Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {
    }

    Vector3 operator+(const Vector3 &r) const {
        return {x + r.x, y + r.y, z + r.z};
    }
    Vector3 operator-(const Vector3 &r) const {
        return {x - r.x, y - r.y, z - r.z};
    }
    Vector3 operator*(float s) const {
        return {x * s, y * s, z * s};
    }

    Vector3 &operator+=(const Vector3 &r) {
        x += r.x;
        y += r.y;
        z += r.z;
        return *this;
    }
};
