#define NOMINMAX
#include "Player.h"
#include "Camera.h"
#include "Input.h"

using namespace DirectX;

void Player::Initialize(ID3D12Device *device, const std::string &objPath, const Input *input) {
	// プレイヤー
	model_.Initialize(device, objPath);

	// 入力
	input_ = input;
}

void Player::Update(float deltaTime) {
    Move_(deltaTime);

	// 位置更新
	transform_.pos.x += velocity_.x;
	transform_.pos.y += velocity_.y;
	transform_.pos.z += velocity_.z;
}

void Player::Finalize() {}

void Player::Move_(float deltaTime) {
    // 入力ベクトル
    XMFLOAT3 moveDir = {0.0f, 0.0f, 0.0f};

    if (input_->IsKeyDown(DIK_D)) moveDir.x += 1.0f;
    if (input_->IsKeyDown(DIK_A)) moveDir.x -= 1.0f;

    // ベクトル長を計算して正規化
    XMVECTOR v = XMLoadFloat3(&moveDir);
    float len = XMVectorGetX(XMVector3Length(v));

    if (len > 0.0001f) {
        v = XMVector3Normalize(v);

        // 移動速度
        v = XMVectorScale(v, speed_ * deltaTime);

        XMStoreFloat3(&velocity_, v);
    } else {
        // 入力がなければ止まる
        velocity_ = {0.0f, 0.0f, 0.0f};
    }
}
