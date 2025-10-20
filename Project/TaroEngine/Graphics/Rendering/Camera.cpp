#include "Camera.h"
#include <algorithm>

Camera::Camera() { Reset(); }

void Camera::Reset() noexcept {
    eye_ = {0.0f, 1.5f, -3.0f};
    tgt_ = {0.0f, 0.5f,  0.0f};
    up_ = {0.0f, 1.0f,  0.0f};
    fovYDeg_ = 60.0f; nearZ_ = 0.1f; farZ_ = 100.0f;
    aspect_ = 16.0f / 9.0f;
    viewDirty_ = projDirty_ = true;
}

void Camera::SetPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept {
    fovYDeg_ = std::clamp(fovYDeg, 1.0f, 160.0f);
    aspect_ = std::max(1e-6f, aspect);
    nearZ_ = std::max(1e-4f, nearZ);
    farZ_ = std::max(nearZ_ + 1e-4f, farZ);
    projDirty_ = true;
}

void Camera::SetViewportSize(uint32_t w, uint32_t h) noexcept {
    const float fw = static_cast<float>(std::max(1u, w));
    const float fh = static_cast<float>(std::max(1u, h));
    aspect_ = fw / fh;
    projDirty_ = true;
}

void Camera::LookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up) noexcept {
    DirectX::XMStoreFloat3(&eye_, eye);
    DirectX::XMStoreFloat3(&tgt_, target);
    DirectX::XMStoreFloat3(&up_, up);
    viewDirty_ = true;
}

void Camera::SetEye(const DirectX::XMFLOAT3 &e) noexcept { eye_ = e; viewDirty_ = true; }
void Camera::SetTarget(const DirectX::XMFLOAT3 &t) noexcept { tgt_ = t; viewDirty_ = true; }
void Camera::SetUp(const DirectX::XMFLOAT3 &u) noexcept { up_ = u; viewDirty_ = true; }

void Camera::YawPitch(float yawRad, float pitchRad) noexcept {
    using namespace DirectX;
    const XMVECTOR eyeV = XMLoadFloat3(&eye_);
    const XMVECTOR tgtV = XMLoadFloat3(&tgt_);
    const XMVECTOR upV = XMLoadFloat3(&up_);

    XMVECTOR view = XMVector3Normalize(XMVectorSubtract(tgtV, eyeV));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(upV, view));

    // pitch then yaw
    view = XMVector3TransformNormal(view, XMMatrixRotationAxis(right, pitchRad));
    view = XMVector3TransformNormal(view, XMMatrixRotationAxis(upV, yawRad));

    XMVECTOR newTgt = XMVectorAdd(eyeV, XMVector3Normalize(view));
    XMStoreFloat3(&tgt_, newTgt);
    viewDirty_ = true;
}

void Camera::Orbit(float yawRad, float pitchRad, float radiusScale) noexcept {
    using namespace DirectX;
    XMVECTOR eyeV = XMLoadFloat3(&eye_);
    XMVECTOR tgtV = XMLoadFloat3(&tgt_);
    XMVECTOR upV = XMLoadFloat3(&up_);

    XMVECTOR toEye = XMVectorSubtract(eyeV, tgtV);
    float r = XMVectorGetX(XMVector3Length(toEye));
    XMVECTOR dir = XMVector3Normalize(toEye);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(upV, dir));
    dir = XMVector3TransformNormal(dir, XMMatrixRotationAxis(right, pitchRad));
    dir = XMVector3TransformNormal(dir, XMMatrixRotationAxis(upV, yawRad));
    r *= std::max(0.0001f, radiusScale);

    XMVECTOR newEye = XMVectorAdd(tgtV, XMVectorScale(dir, r));
    XMStoreFloat3(&eye_, newEye);
    viewDirty_ = true;
}

void Camera::Dolly(float delta) noexcept {
    using namespace DirectX;
    XMVECTOR eyeV = XMLoadFloat3(&eye_);
    XMVECTOR tgtV = XMLoadFloat3(&tgt_);
    XMVECTOR dir = XMVector3Normalize(XMVectorSubtract(tgtV, eyeV));
    XMVECTOR off = XMVectorScale(dir, delta);
    eyeV = XMVectorAdd(eyeV, off);
    tgtV = XMVectorAdd(tgtV, off);
    XMStoreFloat3(&eye_, eyeV);
    XMStoreFloat3(&tgt_, tgtV);
    viewDirty_ = true;
}

void Camera::Pan(float dx, float dy) noexcept {
    using namespace DirectX;
    XMVECTOR eyeV = XMLoadFloat3(&eye_);
    XMVECTOR tgtV = XMLoadFloat3(&tgt_);
    XMVECTOR upV = XMLoadFloat3(&up_);
    XMVECTOR view = XMVector3Normalize(XMVectorSubtract(tgtV, eyeV));
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(upV, view));
    XMVECTOR move = XMVectorAdd(XMVectorScale(right, dx), XMVectorScale(upV, dy));
    eyeV = XMVectorAdd(eyeV, move);
    tgtV = XMVectorAdd(tgtV, move);
    XMStoreFloat3(&eye_, eyeV);
    XMStoreFloat3(&tgt_, tgtV);
    viewDirty_ = true;
}

const DirectX::XMMATRIX &Camera::View() noexcept {
    if (viewDirty_) RecalcView_();
    return view_;
}

const DirectX::XMMATRIX &Camera::Proj() noexcept {
    if (projDirty_) RecalcProj_();
    return proj_;
}

DirectX::XMMATRIX Camera::ViewProj() noexcept {
    return DirectX::XMMatrixMultiply(View(), Proj());
}

void Camera::StoreT(float out16[16], const DirectX::XMMATRIX &m) noexcept {
    using namespace DirectX;
    const XMMATRIX mt = XMMatrixTranspose(m);
    XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4 *>(out16), mt);
}

void Camera::GetTransposeVP(float vT[16], float pT[16]) noexcept {
    StoreT(vT, View());
    StoreT(pT, Proj());
}

void Camera::RecalcView_() noexcept {
    using namespace DirectX;
    view_ = XMMatrixLookAtLH(XMLoadFloat3(&eye_), XMLoadFloat3(&tgt_), XMLoadFloat3(&up_));
    viewDirty_ = false;
}

void Camera::RecalcProj_() noexcept {
    using namespace DirectX;
    proj_ = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovYDeg_), aspect_, nearZ_, farZ_);
    projDirty_ = false;
}
