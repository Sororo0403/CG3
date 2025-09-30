#pragma once
#include <d3d12.h>
#include <string>
#include <wrl.h>

class ShaderCompiler; // 前方宣言

/// <summary>
/// スプライト描画の「共通描画ルール」（RootSignature / PSO）を一括管理し、
/// 毎フレームの描画前にまとめて設定できるユーティリティ。
/// </summary>
class SpriteCommon {
public:
	/// <summary>
	/// PSO 作成時に使用するフォーマット設定をまとめた構造体。
	/// </summary>
	struct PipelineFormats {
		DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM; ///< レンダーターゲットのフォーマット
		DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;   ///< 深度ステンシルのフォーマット（未使用なら UNKNOWN）
		UINT numRenderTargets = 1; ///< MRT のレンダーターゲット数
	};

public:
	SpriteCommon() = default;
	~SpriteCommon() = default;

	/// <summary>
	/// 初期化処理。Device を保持し、RootSignature を作成する。
	/// </summary>
	/// <param name="device">Direct3D デバイス</param>
	void Initialize(ID3D12Device *device);

	/// <summary>
	/// グラフィックスパイプライン（PSO）を生成する。
	/// </summary>
	/// <param name="compiler">ShaderCompiler（DXC ラッパー）</param>
	/// <param name="device">Direct3D デバイス</param>
	/// <param name="vsPath">頂点シェーダファイルのパス</param>
	/// <param name="psPath">ピクセルシェーダファイルのパス</param>
	/// <param name="formats">RTV/DSV のフォーマット設定</param>
	/// <param name="vsEntry">頂点シェーダのエントリポイント（既定: L"main"）</param>
	/// <param name="psEntry">ピクセルシェーダのエントリポイント（既定: L"main"）</param>
	void CreateGraphicsPipeline(
		ShaderCompiler &compiler,
		ID3D12Device *device,
		const std::wstring &vsPath,
		const std::wstring &psPath,
		const PipelineFormats &formats = {},
		const std::wstring &vsEntry = L"main",
		const std::wstring &psEntry = L"main");

	/// <summary>
	/// 共通の描画設定（RS/PSO/トポロジ）をコマンドリストに適用する。
	/// </summary>
	/// <param name="cmd">描画先のコマンドリスト</param>
	/// <param name="topology">プリミティブトポロジ（デフォルト: 三角形リスト）</param>
	void ApplyCommonDrawSettings(
		ID3D12GraphicsCommandList *cmd,
		D3D12_PRIMITIVE_TOPOLOGY topology =
		D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST) const;

	/// <summary>作成済みの RootSignature を取得する。</summary>
	ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }

	/// <summary>作成済みの PipelineState を取得する。</summary>
	ID3D12PipelineState *GetPipelineState() const { return pipelineState_.Get(); }

private:
	/// <summary>
	/// RootSignature を作成する。
	/// </summary>
	/// <param name="device">	Direct3D デバイス</param>
	void CreateRootSignature(ID3D12Device *device);

private:
	ID3D12Device *device_ = nullptr; ///< D3D12 デバイス（借用）

	Microsoft::WRL::ComPtr<ID3D12RootSignature>	rootSignature_; ///< ルートシグネチャ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; ///< パイプラインステート
};
