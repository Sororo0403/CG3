#pragma once
#include "Matrix4x4.h"
#include "Vector3.h"
#include "MatrixUtil.h"

/// <summary>
/// 3D カメラ。位置 / 注視点 / 上方向 とレンズパラメータ（FOV / アスペクト / 近遠）から
/// View / Projection / ViewProjection を生成・保持する。
/// </summary>
class Camera {
public:
    /// <summary>
    /// コンストラクタ。
    /// </summary>
    Camera() = default;

    /// <summary>
    /// デストラクタ。
    /// </summary>
    ~Camera() = default;

    /// <summary>
    /// 初期化。<br/>
    /// 画面サイズからアスペクト比を設定し、既定のレンズを組む。
    /// </summary>
    /// <param name="viewportWidth">ビューポート幅</param>
    /// <param name="viewportHeight">ビューポート高さ</param>
    /// <param name="fovYRadians">垂直 FOV（ラジアン）</param>
    /// <param name="nearZ">ニア平面</param>
    /// <param name="farZ">ファー平面</param>
    void Initialize(
        float viewportWidth,
        float viewportHeight,
        float fovYRadians = 60.0f * 3.1415926535f / 180.0f,
        float nearZ = 0.1f,
        float farZ = 1000.0f);

    /// <summary>
    /// 毎フレーム更新。dirty のとき行列を再計算する。
    /// </summary>
    void Update();

    /// <summary>
    /// ビューポートサイズの変更（リサイズ）を反映。
    /// </summary>
    void SetViewportSize(float w, float h);

    /// <summary>レンズ（FOV/アスペクト/近遠）を設定。</summary>
    void SetLens(float fovYRadians, float aspect, float nearZ, float farZ);

    /// <summary>位置を設定。</summary>
    void SetPosition(const Vector3 &pos);

    /// <summary>注視点を設定。</summary>
    void SetTarget(const Vector3 &target);

    /// <summary>上方向を設定。</summary>
    void SetUp(const Vector3 &up);

    /// <summary>
    /// ヨー / ピッチ（ラジアン）から向きを指定。<br/>
    /// 右手系の角度約束に近い直感：Yaw は+で左回り（Y軸周り）、Pitch は+で上向き（X軸周り）。
    /// </summary>
    /// <param name="yaw">Y軸周り</param>
    /// <param name="pitch">X軸周り</param>
    void SetYawPitch(float yaw, float pitch);

    /// <summary>指定ベクトルだけカメラ位置・注視点を平行移動。</summary>
    void Translate(const Vector3 &delta);

    /// <summary>原点または任意中心を軸に公転（ターゲットを中心として位置を回す）。</summary>
    void OrbitTarget(float yawDelta, float pitchDelta, float radius);

    /// <summary>現在の位置を取得。</summary>
    const Vector3 &GetPosition() const { return position_; }

    /// <summary>現在の注視点を取得。</summary>
    const Vector3 &GetTarget() const { return target_; }

    /// <summary>現在の上方向を取得。</summary>
    const Vector3 &GetUp() const { return up_; }

    /// <summary>View 行列を取得。</summary>
    const Matrix4x4 &GetView() const { return view_; }

    /// <summary>Projection 行列を取得。</summary>
    const Matrix4x4 &GetProjection() const { return proj_; }

    /// <summary>ViewProjection 行列を取得。</summary>
    const Matrix4x4 &GetViewProjection() const { return viewProj_; }

    /// <summary>FOV（ラジアン）を取得。</summary>
    float GetFovY() const { return fovY_; }

    /// <summary>アスペクト比を取得。</summary>
    float GetAspect() const { return aspect_; }

    /// <summary>ニア平面を取得。</summary>
    float GetNearZ() const { return nearZ_; }

    /// <summary>ファー平面を取得。</summary>
    float GetFarZ() const { return farZ_; }

private:
    // パラメータ
    Vector3 position_{0.0f, 0.0f, -5.0f};
    Vector3 target_{0.0f, 0.0f,  0.0f};
    Vector3 up_{0.0f, 1.0f,  0.0f};

    float fovY_ = 60.0f * 3.1415926535f / 180.0f;
    float aspect_ = 16.0f / 9.0f;
    float nearZ_ = 0.1f;
    float farZ_ = 1000.0f;

    // 行列
    Matrix4x4 view_ = MatrixUtil::MakeIdentityMatrix();
    Matrix4x4 proj_ = MatrixUtil::MakeIdentityMatrix();
    Matrix4x4 viewProj_ = MatrixUtil::MakeIdentityMatrix();

    bool dirty_ = true;

    /// <summary>
    /// 内部計算：View/Proj/ViewProj を再生成。
    /// </summary>
    void Recalculate_();

    /// <summary>
    /// Yaw/Pitch から forward を作って target を更新。
    /// </summary>
    void ApplyYawPitch_(float yaw, float pitch);
};
