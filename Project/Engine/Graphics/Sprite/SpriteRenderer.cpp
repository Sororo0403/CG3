#include "SpriteRenderer.h"
#include "DirectX/DirectXCommon.h"
#include <cassert>
#include <cmath>

using namespace Microsoft::WRL;

// 簡易行列ユーティリティ
static void MakeIdentity4x4(float m[4][4]) {
	std::memset(m, 0, sizeof(float) * 16);
	for (int i = 0; i < 4; ++i) m[i][i] = 1.0f;
}

void SpriteRenderer::Initialize(DirectXCommon *dx, ShaderCompiler *shaderCompiler) {
	dx_ = dx;
	shaderCompiler_ = shaderCompiler;
	CreatePipeline_();
	CreateGeometry_();
	CreateConstantBuffer_();
}

void SpriteRenderer::CreatePipeline_() {
	ID3D12Device *device = dx_->GetDevice();

	// RootSignature: b0 (CBV), t0 (SRV), s0 (StaticSampler)
	D3D12_ROOT_PARAMETER rootParams[2] = {};

	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_DESCRIPTOR_RANGE range{};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.BaseShaderRegister = 0;
	range.NumDescriptors = 1;
	range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[1].DescriptorTable.pDescriptorRanges = &range;
	rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC sampler{};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ShaderRegister = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;

	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	desc.NumParameters = _countof(rootParams);
	desc.pParameters = rootParams;
	desc.NumStaticSamplers = 1;
	desc.pStaticSamplers = &sampler;

	ComPtr<ID3DBlob> sigBlob;
	ComPtr<ID3DBlob> errBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
	if (FAILED(hr)) {
		if (errBlob) OutputDebugStringA((char *)errBlob->GetBufferPointer());
		assert(false);
	}

	hr = device->CreateRootSignature(
		0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	ComPtr<IDxcBlob> vs = shaderCompiler_->Compile(L"Resources/Shaders/Sprite.VS.hlsl", L"vs_6_0");
	ComPtr<IDxcBlob> ps = shaderCompiler_->Compile(L"Resources/Shaders/Sprite.PS.hlsl", L"ps_6_0");

	D3D12_INPUT_ELEMENT_DESC inputElems[2]{};
	inputElems[0].SemanticName = "POSITION";
	inputElems[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElems[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElems[1].SemanticName = "TEXCOORD";
	inputElems[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElems[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayout{};
	inputLayout.NumElements = _countof(inputElems);
	inputLayout.pInputElementDescs = inputElems;

	D3D12_BLEND_DESC blend{};
	blend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	blend.RenderTarget[0].BlendEnable = TRUE;
	blend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	D3D12_RASTERIZER_DESC raster{};
	raster.CullMode = D3D12_CULL_MODE_NONE;
	raster.FillMode = D3D12_FILL_MODE_SOLID;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
	pso.pRootSignature = rootSignature_.Get();
	pso.InputLayout = inputLayout;
	pso.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
	pso.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
	pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pso.NumRenderTargets = 1;
	pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pso.BlendState = blend;
	pso.RasterizerState = raster;
	pso.SampleDesc.Count = 1;
	pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pso.DepthStencilState.DepthEnable = FALSE;
	pso.DepthStencilState.StencilEnable = FALSE;

	hr = device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void SpriteRenderer::CreateGeometry_() {
	ID3D12Device *device = dx_->GetDevice();

	// 頂点(左下, 左上, 右下, 右上) 位置は後で行列でスケーリングするので NDC基準
	SpriteVertex vertices[4] = {
		{-0.5f, -0.5f, 0.0f, 0.0f, 0.0f}, // 左下
		{-0.5f,  0.5f, 0.0f, 0.0f, 1.0f}, // 左上
		{ 0.5f, -0.5f, 0.0f, 1.0f, 0.0f}, // 右下
		{ 0.5f,  0.5f, 0.0f, 1.0f, 1.0f}, // 右上
	};

	uint32_t indices[6] = {0,1,2, 1,3,2};

	// アップロードバッファ（簡易）
	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC vdesc{};
	vdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vdesc.Width = sizeof(vertices);
	vdesc.Height = 1;
	vdesc.DepthOrArraySize = 1;
	vdesc.MipLevels = 1;
	vdesc.SampleDesc.Count = 1;
	vdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &vdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&vertexBuffer_));
	assert(SUCCEEDED(hr));

	void *mapped = nullptr;
	vertexBuffer_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, vertices, sizeof(vertices));
	vertexBuffer_->Unmap(0, nullptr);

	vbView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
	vbView_.SizeInBytes = sizeof(vertices);
	vbView_.StrideInBytes = sizeof(SpriteVertex);

	D3D12_RESOURCE_DESC idesc = vdesc;
	idesc.Width = sizeof(indices);
	hr = device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &idesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&indexBuffer_));
	assert(SUCCEEDED(hr));

	indexBuffer_->Map(0, nullptr, &mapped);
	std::memcpy(mapped, indices, sizeof(indices));
	indexBuffer_->Unmap(0, nullptr);

	ibView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
	ibView_.SizeInBytes = sizeof(indices);
	ibView_.Format = DXGI_FORMAT_R32_UINT;
}

void SpriteRenderer::CreateConstantBuffer_() {
	ID3D12Device *device = dx_->GetDevice();

	D3D12_HEAP_PROPERTIES heap{};
	heap.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC desc{};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Width = (sizeof(SpriteConstant) + 0xFF) & ~0xFF; // 256アライン
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&constantBuffer_));
	assert(SUCCEEDED(hr));

	constantBuffer_->Map(0, nullptr, reinterpret_cast<void **>(&cbData_));
}

// 画面幅/高さ w,h と スプライト座標 x,y,width,height から WVPを作る
void SpriteRenderer::MakeOrthoWVP_(
	float screenW, float screenH,
	float x, float y, float width, float height,
	float out[4][4]) {

	// オルソ射影 (画面座標 0〜w,0〜h -> NDC)
	float l = 0.0f;
	float r = screenW;
	float t = 0.0f;
	float b = screenH;
	float n = 0.0f;
	float f = 1.0f;

	float mProj[4][4]{};
	MakeIdentity4x4(mProj);
	mProj[0][0] = 2.0f / (r - l);
	mProj[1][1] = -2.0f / (b - t);
	mProj[2][2] = 1.0f / (f - n);
	mProj[3][0] = -(r + l) / (r - l);
	mProj[3][1] = (b + t) / (b - t);
	mProj[3][2] = -n / (f - n);

	// スプライトのモデル行列 (画面座標系)
	float mWorld[4][4]{};
	MakeIdentity4x4(mWorld);
	mWorld[0][0] = width;
	mWorld[1][1] = height;
	mWorld[3][0] = x + width * 0.5f;
	mWorld[3][1] = y + height * 0.5f;
	mWorld[3][2] = 0.0f;

	// out = world * proj
	float tmp[4][4]{};
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			tmp[i][j] =
				mWorld[i][0] * mProj[0][j] +
				mWorld[i][1] * mProj[1][j] +
				mWorld[i][2] * mProj[2][j] +
				mWorld[i][3] * mProj[3][j];
		}
	}
	std::memcpy(out, tmp, sizeof(tmp));
}

void SpriteRenderer::DrawSprite(
	uint32_t textureId,
	float x, float y,
	float width, float height,
	float u0, float v0,
	float uSize, float vSize,
	const float color[4]) {

	ID3D12GraphicsCommandList *cmd = dx_->GetCommandList();

	const auto &tex = TextureManager::GetTexture(textureId);

	// CB 更新
	float screenW = static_cast<float>(1280);
	float screenH = static_cast<float>(720);
	MakeOrthoWVP_(screenW, screenH, x, y, width, height, cbData_->mWVP);

	cbData_->color[0] = color[0];
	cbData_->color[1] = color[1];
	cbData_->color[2] = color[2];
	cbData_->color[3] = color[3];

	cbData_->uvRect[0] = u0;
	cbData_->uvRect[1] = v0;
	cbData_->uvRect[2] = uSize;
	cbData_->uvRect[3] = vSize;

	cmd->SetPipelineState(pipelineState_.Get());
	cmd->SetGraphicsRootSignature(rootSignature_.Get());

	ID3D12DescriptorHeap *heaps[] = {dx_->GetSrvHeap()};
	cmd->SetDescriptorHeaps(1, heaps);

	cmd->IASetVertexBuffers(0, 1, &vbView_);
	cmd->IASetIndexBuffer(&ibView_);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	cmd->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
	cmd->SetGraphicsRootDescriptorTable(1, tex.gpuHandle);

	cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
