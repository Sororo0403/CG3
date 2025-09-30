#pragma once
#include <d3d12.h>
#include <wrl.h>
#include "Material.h"
#include "TransformMatrix.h"
#include "VertexData.h"

/// <summary>
/// スプライトクラス
/// </summary>
class Sprite {
public:
	Sprite() = default;
	~Sprite() = default;

	/// <summary>
	/// 初期化（リソース生成とマップ）
	/// </summary>
	void Initialize(ID3D12Device *device);

	/// <summary>
	/// 毎フレーム更新
	/// </summary>
	void Update();

	/// <summary>
	/// 描画
	/// </summary>
	void Draw(ID3D12GraphicsCommandList *cmdList);

private:
	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;

	// バッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

	// CPU からアクセスするためのポインタ
	VertexData *vertexData_ = nullptr;
	uint32_t *indexData_ = nullptr;
	Material *materialData_ = nullptr;
	TransformMatrix *transformMatrixData_ = nullptr;

	// スプライト変換パラメータ
	Vector2 position_ = {0.0f, 0.0f};
	Vector2 size_ = {100.0f, 100.0f};
	float rotation_ = 0.0f;
};
