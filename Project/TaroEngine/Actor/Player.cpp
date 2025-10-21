#define NOMINMAX
#include "Player.h"
#include "Camera.h"
#include <DirectXMath.h>

void Player::Initialize(ID3D12Device *device, const std::string &objPath) {
	model_.Initialize(device, objPath);
	transform_ = {};
}

void Player::Update(float /*deltaTime*/) {}

void Player::Finalize() {}
