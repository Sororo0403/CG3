#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include "Transform.h"

class Camera {
public:

    /// <summary>
    /// カメラを初期化する（最初は透視扱いだが、後でSetOrtho呼べば正射影に切り替わる）。
    /// eye: 位置（ワールド）
    /// yawPitchRoll: {pitch,yaw,roll} [rad]
    /// fovYDeg/aspect/nearZ/farZ: 透視投影用パラメータ
    /// </summary>
    void Initialize(const DirectX::XMFLOAT3 &eye = {0,0,-5},
        const DirectX::XMFLOAT3 &yawPitchRoll = {0,0,0},
        float fovYDeg = 60.0f,
        float aspect = 16.0f / 9.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f) noexcept;

    /// <summary>
    /// 透視投影にする
    /// </summary>
    void SetPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept;

    /// <summary>
    /// 正射影にする (2Dっぽい等倍表示)
    /// viewWidth/viewHeight はワールド座標で画面に収めたい可視範囲サイズ
    /// nearZ/farZ は深度範囲
    /// </summary>
    void SetOrtho(float viewWidth, float viewHeight, float nearZ, float farZ) noexcept;

    /// <summary>
    /// 正射影中の表示範囲を更新（画面リサイズに合わせる）
    /// </summary>
    void SetOrthoViewSize(float viewWidth, float viewHeight) noexcept;

    /// <summary>
    /// 今オーソ（正射影）か？
    /// </summary>
    bool IsOrtho() const noexcept { return useOrtho_; }

    /// <summary>
    /// ビューポートサイズからアスペクトを更新する。
    /// 透視投影では aspect を更新する。
    /// 正射影ではここでは比率に使わず、GameScene 側でorthoWidth/Heightを決め直す。
    /// </summary>
    void SetViewportSize(uint32_t w, uint32_t h) noexcept;

    /// <summary>位置を設定</summary>
    void SetPos(const DirectX::XMFLOAT3 &p) noexcept;

    /// <summary>回転を設定（pitch,yaw,roll）</summary>
    void SetRot(const DirectX::XMFLOAT3 &pitchYawRollRad) noexcept;

    /// <summary>LookAtで向きを決める（roll=0固定）</summary>
    void LookAt(const DirectX::XMFLOAT3 &eye,
        const DirectX::XMFLOAT3 &target,
        const DirectX::XMFLOAT3 &up = {0,1,0}) noexcept;

    DirectX::XMVECTOR GetForward() const noexcept;
    DirectX::XMVECTOR GetRight() const noexcept;
    DirectX::XMVECTOR GetUp() const noexcept;

    /// <summary>ローカル軸で移動（右・上・前）</summary>
    void MoveLocal(float right, float up, float forward) noexcept;
    /// <summary>ワールド軸で移動（x,y,z）</summary>
    void MoveWorld(float dx, float dy, float dz) noexcept;
    /// <summary>前後移動（ローカルZ）</summary>
    void Dolly(float delta) noexcept { MoveLocal(0, 0, delta); }
    /// <summary>スクリーン平面パン（ローカル右・上）</summary>
    void Pan(float dx, float dy) noexcept { MoveLocal(dx, dy, 0); }
    /// <summary>ヨー/ピッチの加算（FPS的回転）</summary>
    void AddYawPitch(float yawRad, float pitchRad) noexcept;
    /// <summary>任意点を中心にオービット（公転）</summary>
    void OrbitAround(const DirectX::XMFLOAT3 &center,
        float yawDelta, float pitchDelta,
        float radiusScale = 1.0f) noexcept;

    // ===== 行列取得 =====
    const DirectX::XMMATRIX &GetView() const noexcept;
    const DirectX::XMMATRIX &GetProj() const noexcept;

    // ===== レンズパラメータ取得 =====
    float GetFovYDeg() const noexcept { return fovYDeg_; }
    float GetNearZ()   const noexcept { return nearZ_; }
    float GetFarZ()    const noexcept { return farZ_; }
    float GetAspect()  const noexcept { return aspect_; }

private:
    void RecalcView_() const noexcept;
    void RecalcProj_() const noexcept;
    static float ClampPitch_(float pitch) noexcept;

private:
    Transform transform_{};   // pos / rot(pitch,yaw,roll) / scale(未使用)

    // 透視投影パラメータ
    float fovYDeg_ = 60.0f;
    float aspect_ = 16.0f / 9.0f;

    // 共通Zレンジ
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;

    // 正射影パラメータ
    bool  useOrtho_ = false;
    float orthoWidth_ = 10.0f;
    float orthoHeight_ = 10.0f;

    mutable DirectX::XMMATRIX view_{DirectX::XMMatrixIdentity()};
    mutable DirectX::XMMATRIX proj_{DirectX::XMMatrixIdentity()};
    mutable bool viewDirty_ = true;
    mutable bool projDirty_ = true;
};
