#pragma once

#include <DirectXMath.h>

/// <summary>
/// 3D カメラ（最小構成）
/// </summary>
class Camera {
  public:
    Camera() = default;

    // Setter
    void SetPerspective(float fovY, float aspect, float nearZ, float farZ);
    void SetOrthographic(float width, float height, float nearZ, float farZ);
    void SetPosition(const DirectX::XMFLOAT3 &pos) {
        position_ = pos;
    }
    void SetRotation(const DirectX::XMFLOAT3 &rot) {
        rotation_ = rot;
    }

    // Getter
    DirectX::XMMATRIX GetView() const;
    DirectX::XMMATRIX GetProjection() const;
    DirectX::XMMATRIX GetViewProj() const;
    const DirectX::XMFLOAT3 &GetPosition() const {
        return position_;
    }
    const DirectX::XMFLOAT3 &GetRotation() const {
        return rotation_;
    }

  private:
    // Transform
    DirectX::XMFLOAT3 position_{0.0f, 0.0f, -5.0f};
    DirectX::XMFLOAT3 rotation_{0.0f, 0.0f, 0.0f};

    // Projection
    DirectX::XMMATRIX projection_{DirectX::XMMatrixIdentity()};
};
