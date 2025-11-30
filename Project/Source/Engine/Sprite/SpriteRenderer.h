#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "Texture/TextureManager.h"
#include "Shader/ShaderCompiler.h"

struct SpriteVertex {
	float x, y, z;
	float u, v;
};

struct SpriteConstant {
	float mWVP[4][4];
	float color[4];
	float uvRect[4]; // u0, v0, uSize, vSize
};

class DirectXCommon;

class SpriteRenderer {
public:
	void Initialize(DirectXCommon *dx, ShaderCompiler *shaderCompiler);
	void DrawSprite(
		uint32_t textureId,
		float x, float y,
		float width, float height,
		float u0, float v0,
		float uSize, float vSize,
		const float color[4]);

private:
	DirectXCommon *dx_ = nullptr;
	ShaderCompiler *shaderCompiler_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;

	D3D12_VERTEX_BUFFER_VIEW vbView_{};
	D3D12_INDEX_BUFFER_VIEW ibView_{};
	SpriteConstant *cbData_ = nullptr;

	void CreatePipeline_();
	void CreateGeometry_();
	void CreateConstantBuffer_();

	void MakeOrthoWVP_(float w, float h, float x, float y, float width, float height, float (*out)[4]);
};
