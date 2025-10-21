// Camera.h
#pragma once
#include <DirectXMath.h>
#include <cstdint>

class Camera {
public:
    Camera();

    // --- 構築 / リセット ---
    void Reset() noexcept;

    // --- レンズ / ビューポート設定 ---
    void SetPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept;
    void SetViewportSize(uint32_t w, uint32_t h) noexcept;

    // --- 位置 / 姿勢 ---
    void LookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up) noexcept;
    void SetEye(const DirectX::XMFLOAT3 &e) noexcept;
    void SetTarget(const DirectX::XMFLOAT3 &t) noexcept;
    void SetUp(const DirectX::XMFLOAT3 &u) noexcept;

    const DirectX::XMFLOAT3 &GetEye()    const noexcept { return eye_; }
    const DirectX::XMFLOAT3 &GetTarget() const noexcept { return tgt_; }
    const DirectX::XMFLOAT3 &GetUp()     const noexcept { return up_; }

    float GetFovYDeg() const noexcept { return fovYDeg_; }
    float GetNearZ()   const noexcept { return nearZ_; }
    float GetFarZ()    const noexcept { return farZ_; }
    float GetAspect()  const noexcept { return aspect_; }

    // --- 操作 ---
    void YawPitch(float yawRad, float pitchRad) noexcept;
    void Orbit(float yawRad, float pitchRad, float radiusScale = 1.0f) noexcept;
    void Dolly(float delta) noexcept;
    void Pan(float dx, float dy) noexcept;

    const DirectX::XMMATRIX &GetViewMatrix() const noexcept;
    const DirectX::XMMATRIX &GetProjMatrix() const noexcept;

private:
    void RecalcViewMatrix_() const noexcept;
    void RecalcProjMatrix_() const noexcept;

private:
    DirectX::XMFLOAT3 eye_{};
    DirectX::XMFLOAT3 tgt_{};
    DirectX::XMFLOAT3 up_{};

    float fovYDeg_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 100.0f;

    // 遅延更新（constメソッドでも更新できるようにmutable）
    mutable DirectX::XMMATRIX view_{DirectX::XMMatrixIdentity()};
    mutable DirectX::XMMATRIX proj_{DirectX::XMMatrixIdentity()};
    mutable bool viewDirty_ = true;
    mutable bool projDirty_ = true;
};
