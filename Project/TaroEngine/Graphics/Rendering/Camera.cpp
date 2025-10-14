#include "Camera.h"
#include "VectorUtil.h"
#include "MatrixUtil.h"
#include <cmath>

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
