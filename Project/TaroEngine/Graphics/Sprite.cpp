#include "Sprite.h"
#include "BufferUtil.h"
#include "MatrixUtil.h"  
#include <cassert>
#include <cstring>   

// HLSL 側が row_major なら 1 にする（転置せずそのまま送る）
// 既定の column_major のままなら 0（送る直前に Transpose する）
#define SPRITE_SEND_ROW_MAJOR 0

void Sprite::Initialize(ID3D12Device *device) {
	assert(device);

	// === 頂点バッファ ===
	vertexResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(VertexData) * 4);
	indexResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(uint32_t) * 6);

	// マップ
	vertexResource_->Map(0, nullptr, reinterpret_cast<void **>(&vertexData_));
	indexResource_->Map(0, nullptr, reinterpret_cast<void **>(&indexData_));

	// バッファビュー設定
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	// 四角形のインデックス
	uint32_t indices[6] = {0, 1, 2, 2, 1, 3};
	std::memcpy(indexData_, indices, sizeof(indices));

	// === マテリアル ===
	materialResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void **>(&materialData_));
	materialData_->color = Vector4(1, 1, 1, 1);
	materialData_->enableLighting = false;
	materialData_->uvTransform = MatrixUtil::MakeIdentityMatrix();

	// === 座標変換行列 ===
	transformResource_ = BufferUtil::CreateUploadBuffer(device, sizeof(TransformMatrix));
	transformResource_->Map(0, nullptr, reinterpret_cast<void **>(&transformMatrixData_));
	transformMatrixData_->WVP = MatrixUtil::MakeIdentityMatrix();
	transformMatrixData_->World = MatrixUtil::MakeIdentityMatrix();
}

void Sprite::Update() {
	// 頂点座標を更新（位置とサイズを反映）
	float w = size_.x * 0.5f;
	float h = size_.y * 0.5f;

	vertexData_[0] = {{-w, -h, 0.0f}, {0.0f, 1.0f}};
	vertexData_[1] = {{-w,  h, 0.0f}, {0.0f, 0.0f}};
	vertexData_[2] = {{ w, -h, 0.0f}, {1.0f, 1.0f}};
	vertexData_[3] = {{ w,  h, 0.0f}, {1.0f, 0.0f}};

	// ワールド行列（行メジャー / 行ベクトル右掛け）
	auto scale = MatrixUtil::MakeScaleMatrix(size_.x, size_.y, 1.0f);
	auto rot = MatrixUtil::MakeRotationZMatrix(rotation_);
	auto trans = MatrixUtil::MakeTranslationMatrix(position_.x, position_.y, 0.0f);

	auto world = MatrixUtil::Multiply(MatrixUtil::Multiply(scale, rot), trans);
	auto view = MatrixUtil::MakeIdentityMatrix();
	auto proj = MatrixUtil::MakeOrthographicMatrix(1280.0f, 720.0f, 0.0f, 1.0f);

#if SPRITE_SEND_ROW_MAJOR
	transformMatrixData_->World = world;
	transformMatrixData_->WVP = MatrixUtil::Multiply(world, MatrixUtil::Multiply(view, proj));
#else
	// HLSL が column_major の場合は転置して送る
	transformMatrixData_->World = MatrixUtil::Transpose(world);
	auto wvp = MatrixUtil::Multiply(world, MatrixUtil::Multiply(view, proj));
	transformMatrixData_->WVP = MatrixUtil::Transpose(wvp);
#endif
}

void Sprite::Draw(ID3D12GraphicsCommandList *cmdList) {
	assert(cmdList);

	// 頂点・インデックスバッファ設定
	cmdList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	cmdList->IASetIndexBuffer(&indexBufferView_);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファ設定 (RootParameter の順番に応じて設定)
	cmdList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	cmdList->SetGraphicsRootConstantBufferView(1, transformResource_->GetGPUVirtualAddress());

	// 描画
	cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
