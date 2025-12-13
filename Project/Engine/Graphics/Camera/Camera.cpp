#include "Camera.h"

using namespace DirectX;

void Camera::SetPerspective(float fovY, float aspect, float nearZ, float farZ) {
    projection_ = XMMatrixPerspectiveFovLH(fovY, aspect, nearZ, farZ);
}

void Camera::SetOrthographic(float width, float height, float nearZ,
                             float farZ) {
    projection_ = XMMatrixOrthographicLH(width, height, nearZ, farZ);
}

XMMATRIX Camera::GetView() const {
    XMMATRIX T = XMMatrixTranslation(-position_.x, -position_.y, -position_.z);

    XMMATRIX R =
        XMMatrixRotationRollPitchYaw(-rotation_.x, -rotation_.y, -rotation_.z);

    return R * T;
}

XMMATRIX Camera::GetProjection() const {
    return projection_;
}

XMMATRIX Camera::GetViewProj() const {
    return GetView() * projection_;
}
