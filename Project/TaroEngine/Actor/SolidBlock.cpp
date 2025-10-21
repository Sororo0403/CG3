#include "SolidBlock.h"

void SolidBlock::Initialize(ID3D12Device *device, const std::string &objPath) {
	model_.Initialize(device, objPath);
	transform_ = {};
}

void SolidBlock::Update(float /*deltaTime*/) {
}

void SolidBlock::Finalize() {
}
