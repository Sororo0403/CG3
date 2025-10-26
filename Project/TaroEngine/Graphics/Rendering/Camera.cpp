#include "Camera.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

static constexpr float kPI = 3.14159265358979323846f;

void Camera::Initialize(const XMFLOAT3 &eye,
    const XMFLOAT3 &yawPitchRoll,
    float fovYDeg, float aspect, float nearZ, float farZ) noexcept {

    transform_.pos = eye;
    // yawPitchRoll = {pitch,yaw,roll}
    transform_.rot = {yawPitchRoll.x, yawPitchRoll.y, yawPitchRoll.z};

    fovYDeg_ = fovYDeg;
    aspect_ = aspect;
    nearZ_ = nearZ;
    farZ_ = farZ;

    useOrtho_ = false; // 初期状態はパース前提

    viewDirty_ = true;
    projDirty_ = true;
}

void Camera::SetPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept {
    useOrtho_ = false; // 透視モードに戻す
    fovYDeg_ = std::clamp(fovYDeg, 1.0f, 160.0f);
    aspect_ = std::max(1e-6f, aspect);
    nearZ_ = std::max(1e-4f, nearZ);
    farZ_ = std::max(nearZ_ + 1e-4f, farZ);
    projDirty_ = true;
}

void Camera::SetOrtho(float viewWidth, float viewHeight, float nearZ, float farZ) noexcept {
    useOrtho_ = true;
    orthoWidth_ = std::max(1e-4f, viewWidth);
    orthoHeight_ = std::max(1e-4f, viewHeight);
    nearZ_ = nearZ;
    farZ_ = farZ;
    projDirty_ = true;
}

void Camera::SetOrthoViewSize(float viewWidth, float viewHeight) noexcept {
    orthoWidth_ = std::max(1e-4f, viewWidth);
    orthoHeight_ = std::max(1e-4f, viewHeight);
    projDirty_ = true;
}

void Camera::SetViewportSize(uint32_t w, uint32_t h) noexcept {
    float fw = static_cast<float>(std::max(1u, w));
    float fh = static_cast<float>(std::max(1u, h));

    if (useOrtho_) {
        // 正射影の場合、アスペクトで表示範囲を変えるロジックは
        // GameScene 側が都度 SetOrthoViewSize() するのでここではaspect_更新しない
        (void)fw;
        (void)fh;
    } else {
        aspect_ = fw / fh;
        projDirty_ = true;
    }
}

void Camera::SetPos(const XMFLOAT3 &p) noexcept {
    transform_.pos = p;
    viewDirty_ = true;
}

void Camera::SetRot(const XMFLOAT3 &pitchYawRollRad) noexcept {
    transform_.rot = pitchYawRollRad;
    transform_.rot.x = ClampPitch_(transform_.rot.x);
    viewDirty_ = true;
}

void Camera::LookAt(const XMFLOAT3 &eye, const XMFLOAT3 &target, const XMFLOAT3 &up) noexcept {
    // 位置
    transform_.pos = eye;

    // forward から pitch,yaw を復元（左手座標系: +Z が前）
    XMVECTOR f = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&target), XMLoadFloat3(&eye)));
    float fx = XMVectorGetX(f);
    float fy = XMVectorGetY(f);
    float fz = XMVectorGetZ(f);

    float yaw = std::atan2(fx, fz); // 水平回転
    float pitch = std::asin(std::clamp(fy, -1.0f, 1.0f));
    transform_.rot = {ClampPitch_(pitch), yaw, 0.0f};

    (void)up; // rollはここでは0固定

    viewDirty_ = true;
}

XMVECTOR Camera::GetForward() const noexcept {
    XMMATRIX r = XMMatrixRotationRollPitchYaw(transform_.rot.x, transform_.rot.y, transform_.rot.z);
    return XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), r); // LH +Z 前
}

XMVECTOR Camera::GetRight() const noexcept {
    XMMATRIX r = XMMatrixRotationRollPitchYaw(transform_.rot.x, transform_.rot.y, transform_.rot.z);
    return XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), r);
}

XMVECTOR Camera::GetUp() const noexcept {
    XMMATRIX r = XMMatrixRotationRollPitchYaw(transform_.rot.x, transform_.rot.y, transform_.rot.z);
    return XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), r);
}

void Camera::MoveLocal(float right, float up, float forward) noexcept {
    XMVECTOR r = GetRight();
    XMVECTOR u = GetUp();
    XMVECTOR f = GetForward();
    XMVECTOR delta = XMVectorAdd(
        XMVectorAdd(XMVectorScale(r, right), XMVectorScale(u, up)),
        XMVectorScale(f, forward)
    );

    XMVECTOR p = XMLoadFloat3(&transform_.pos);
    p = XMVectorAdd(p, delta);
    XMStoreFloat3(&transform_.pos, p);

    viewDirty_ = true;
}

void Camera::MoveWorld(float dx, float dy, float dz) noexcept {
    transform_.pos.x += dx;
    transform_.pos.y += dy;
    transform_.pos.z += dz;
    viewDirty_ = true;
}

void Camera::AddYawPitch(float yawRad, float pitchRad) noexcept {
    transform_.rot.y += yawRad; // yaw (左右)
    transform_.rot.x = ClampPitch_(transform_.rot.x + pitchRad); // pitch (上下)
    viewDirty_ = true;
}

void Camera::OrbitAround(const XMFLOAT3 &center, float yawDelta, float pitchDelta, float radiusScale) noexcept {
    XMVECTOR c = XMLoadFloat3(&center);
    XMVECTOR p = XMLoadFloat3(&transform_.pos);
    XMVECTOR v = XMVectorSubtract(p, c); // center→eye

    float r = XMVectorGetX(XMVector3Length(v));
    if (r < 1e-6f) r = 1e-6f;
    v = XMVectorScale(v, 1.0f / r);

    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, v));

    XMMATRIX Rpitch = XMMatrixRotationAxis(right, pitchDelta);
    XMMATRIX Ryaw = XMMatrixRotationY(yawDelta);

    v = XMVector3TransformNormal(v, Rpitch);
    v = XMVector3TransformNormal(v, Ryaw);

    r *= std::max(0.0001f, radiusScale);
    p = XMVectorAdd(c, XMVectorScale(v, r));

    XMStoreFloat3(&transform_.pos, p);

    // つねに center を見る
    LookAt(transform_.pos, center);
    viewDirty_ = true;
}

const XMMATRIX &Camera::GetView() const noexcept {
    if (viewDirty_) RecalcView_();
    return view_;
}

const XMMATRIX &Camera::GetProj() const noexcept {
    if (projDirty_) RecalcProj_();
    return proj_;
}

void Camera::RecalcView_() const noexcept {
    XMVECTOR eye = XMLoadFloat3(&transform_.pos);
    XMVECTOR fwd = GetForward();
    XMVECTOR up = GetUp();
    XMVECTOR tgt = XMVectorAdd(eye, fwd);

    view_ = XMMatrixLookAtLH(eye, tgt, up);
    viewDirty_ = false;
}

void Camera::RecalcProj_() const noexcept {
    if (useOrtho_) {
        float w2 = orthoWidth_ * 0.5f;
        float h2 = orthoHeight_ * 0.5f;
        proj_ = XMMatrixOrthographicOffCenterLH(
            -w2, +w2,
            -h2, +h2,
            nearZ_, farZ_);
    } else {
        proj_ = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(fovYDeg_),
            aspect_,
            nearZ_, farZ_);
    }
    projDirty_ = false;
}

float Camera::ClampPitch_(float pitch) noexcept {
    // 真上/真下の万歳ロックを避ける（±89.9度くらい）
    const float limit = 0.5f * kPI - 0.0015f;
    return std::clamp(pitch, -limit, limit);
}
