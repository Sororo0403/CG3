#include "Camera.h"
#include <cmath>

namespace {
    inline Vector3 Normalize(const Vector3 &v) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len <= 0.0f) return Vector3(0, 0, 0);
        return Vector3(v.x / len, v.y / len, v.z / len);
    }
}

void Camera::Initialize(float viewportWidth, float viewportHeight, float fovYRadians, float nearZ, float farZ) {
    aspect_ = (viewportHeight > 0.0f) ? (viewportWidth / viewportHeight) : (16.0f / 9.0f);
    fovY_ = fovYRadians;
    nearZ_ = nearZ;
    farZ_ = farZ;
    dirty_ = true;
    Recalculate_();
}

void Camera::Update() {
    if (dirty_) { Recalculate_(); }
}

void Camera::SetViewportSize(float w, float h) {
    aspect_ = (h > 0.0f) ? (w / h) : aspect_;
    dirty_ = true;
}

void Camera::SetLens(float fovYRadians, float aspect, float nearZ, float farZ) {
    fovY_ = fovYRadians;
    aspect_ = aspect;
    nearZ_ = nearZ;
    farZ_ = farZ;
    dirty_ = true;
}

void Camera::SetPosition(const Vector3 &pos) {
    position_ = pos;
    dirty_ = true;
}

void Camera::SetTarget(const Vector3 &target) {
    target_ = target;
    dirty_ = true;
}

void Camera::SetUp(const Vector3 &up) {
    up_ = Normalize(up);
    dirty_ = true;
}

void Camera::SetYawPitch(float yaw, float pitch) {
    ApplyYawPitch_(yaw, pitch);
    dirty_ = true;
}

void Camera::Translate(const Vector3 &delta) {
    position_.x += delta.x;
    position_.y += delta.y;
    position_.z += delta.z;
    target_.x += delta.x;
    target_.y += delta.y;
    target_.z += delta.z;
    dirty_ = true;
}

void Camera::OrbitTarget(float yawDelta, float pitchDelta, float radius) {
    // target を中心に半径 radius で公転
    Vector3 forward = Vector3(position_.x - target_.x,
        position_.y - target_.y,
        position_.z - target_.z);
    float r = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    if (r <= 1e-5f) r = radius;

    // 現在方向を球面角に分解してから増分
    float yaw = std::atan2(forward.x, forward.z);     // Yaw: +で左回り
    float pitch = std::atan2(forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z));

    yaw += yawDelta;
    pitch += pitchDelta;
    // ピッチ制限（真上/真下奇点回避）
    const float kLimit = 1.55334306f; // 約 89 度
    if (pitch > kLimit) pitch = kLimit;
    if (pitch < -kLimit) pitch = -kLimit;

    // 新しい位置（target 周り）
    float cy = std::cos(yaw), sy = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    Vector3 offset{
        r * cp * sy,  // x
        r * sp,       // y
        r * cp * cy   // z
    };
    position_ = Vector3(target_.x + offset.x, target_.y + offset.y, target_.z + offset.z);
    dirty_ = true;
}

void Camera::Recalculate_() {
    // View（左手：eye, target, up）
    view_ = MatrixUtil::MakeViewMatrix(position_, target_, up_);

    // Projection（左手：縦FOV, aspect, near/far）
    proj_ = MatrixUtil::MakePerspectiveFovMatrix(fovY_, aspect_, nearZ_, farZ_);

    // ViewProjection
    viewProj_ = MatrixUtil::Multiply(view_, proj_);
    dirty_ = false;
}

void Camera::ApplyYawPitch_(float yaw, float pitch) {
    // forward（左手）: +Z 前、Yaw+ で左回り、Pitch+ で上向き
    float cy = std::cos(yaw), sy = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    Vector3 forward{
        cp * sy,   // x
        sp,        // y
        cp * cy    // z
    };
    target_ = Vector3(position_.x + forward.x, position_.y + forward.y, position_.z + forward.z);
}
