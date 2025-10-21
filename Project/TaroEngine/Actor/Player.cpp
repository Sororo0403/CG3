#define NOMINMAX
#include "Player.h"
#include "Input.h"
using namespace DirectX;

void Player::Initialize(ID3D12Device *device, const std::string &objPath, const Input *input) {
    model_.Initialize(device, objPath);
    input_ = input;
    transform_ = {};
    transform_.scale = {1.0f,1.0f,1.0f};
    sizeX_ = 1.0f; sizeY_ = 1.0f;
}

void Player::Update(float /*dt*/) {
    MoveInputOnly_(0.0f);
}

void Player::Finalize() {}

void Player::MoveInputOnly_(float /*dt*/) {
    moveAxisX_ = 0;
    // A/D
    if (input_->IsKeyDown(DIK_A)) moveAxisX_ -= 1;
    if (input_->IsKeyDown(DIK_D)) moveAxisX_ += 1;

    // SPACEエッジ
    static bool prev = false;
    bool now = input_->IsKeyDown(DIK_SPACE);
    jumpPressedEdge_ = (now && !prev);
    prev = now;
}

bool Player::ConsumeJumpPressedEdge() {
    bool r = jumpPressedEdge_;
    jumpPressedEdge_ = false;
    return r;
}
