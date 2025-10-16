#pragma once
#include "EngineContext.h"
#include "RenderContext.h"
#include "ModelRenderer.h"
#include "Mesh.h"
#include "IScene.h"
#include <DirectXMath.h>

class GameScene :public IScene {
public:
	void Initialize(const EngineContext *engineContext, const RenderContext *renderContext);
	void Update(float deltaTime);
	void Draw();
	void Finalize();

private:
	ModelRenderer model_;
	Mesh          cube_;

	DirectX::XMMATRIX world_ = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX view_ = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX proj_ = DirectX::XMMatrixIdentity();

	// === UI で触るパラメータ ===
	DirectX::XMFLOAT3 eye_{0.0f, 1.5f, -3.0f};
	DirectX::XMFLOAT3 tgt_{0.0f, 0.5f,  0.0f};
	DirectX::XMFLOAT3 up_{0.0f, 1.0f,  0.0f};
	float fovYDeg_ = 60.0f;
	float nearZ_ = 0.1f;
	float farZ_ = 100.0f;

	bool  autoSpin_ = true;
	float rotSpeed_ = 1.0f;   // [rad/sec]

	float color_[4] = {1,1,1,1};

	float angleY_ = 0.0f;

	static void StoreT(float out16[16], const DirectX::XMMATRIX &m) {
		using namespace DirectX;
		XMMATRIX mt = XMMatrixTranspose(m);
		XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4 *>(out16), mt);
	}
};
