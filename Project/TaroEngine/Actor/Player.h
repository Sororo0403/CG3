#pragma once
#include "Model.h"
#include "Transform.h"
#include <DirectXMath.h>
#include <string>

class Input;

/// <summary>
/// プレイヤー（描画＋最小限の移動パラメータ）
/// 物理解決（当たり判定）は GameScene 側で行う。
/// </summary>
class Player {
public:
    void Initialize(ID3D12Device *device, const std::string &objPath, const Input *input);
    void Update(float deltaTime); // 入力のみ処理
    void Finalize();

    const Model &GetModel()     const noexcept { return model_; }
    const Transform &GetTransform() const noexcept { return transform_; }
    Transform &RefTransform()       noexcept { return transform_; }

    // 速度アクセス
    DirectX::XMFLOAT3 &Velocity()       noexcept { return velocity_; }
    const DirectX::XMFLOAT3 &Velocity() const noexcept { return velocity_; }

    // ジャンプ意図（立ち上がり）
    bool ConsumeJumpPressedEdge();

    // 当たり判定サイズ（1タイル基準）
    void  SetSize(float w, float h) noexcept { sizeX_ = w; sizeY_ = h; }
    float W() const noexcept { return sizeX_; }
    float H() const noexcept { return sizeY_; }

    // 地上フラグ（コヨーテ管理は GameScene）
    void SetOnGround(bool v) noexcept { onGround_ = v; }
    bool OnGround() const noexcept { return onGround_; }

    // 入力からの左右意図（-1,0,1）
    int  MoveAxisX() const noexcept { return moveAxisX_; }

private:
    void MoveInputOnly_(float dt);

private:
    Transform transform_{};
    Model model_{};
    const Input *input_ = nullptr;

    DirectX::XMFLOAT3 velocity_{0,0,0}; // x:横, y:高さ, z:前(+Z)
    float sizeX_ = 1.0f;
    float sizeY_ = 1.0f;

    // 入力意図
    int  moveAxisX_ = 0;
    bool jumpPressedEdge_ = false;
    bool onGround_ = false;
};
