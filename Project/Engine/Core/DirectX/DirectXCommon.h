#pragma once

#define NOMINMAX
#include <chrono>
#include <cstdint>
#include <thread>
#include <array>
#include <cassert>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>

class WinApp;

class DirectXCommon {
public:
	/// <summary>
	/// デストラクタ
	/// </summary>
	~DirectXCommon();

	/// <summary>
	/// 初期化処理
	/// </summary>
	/// <param name="winApp">使用するWinAppのポインタ</param>
	void Initialize(const WinApp *winApp);

	/// <summary>
	/// 描画前処理
	/// </summary>
	/// <param name="clearColor"></param>
	void PreDraw(const float clearColor[4]);

	/// <summary>
	/// 描画終了処理
	/// </summary>
	void PostDraw();

	/// <summary>
	/// GPUの実行が全部終わるまで待機
	/// </summary>
	void WaitForGpu();

	// Getter
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap, UINT index) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap, UINT index) const;
	UINT GetSrvDescriptorSize() const { return descriptorSizeSRV_; }
	ID3D12Device *GetDevice() const { return device_.Get(); }
	ID3D12GraphicsCommandList *GetCommandList() const { return commandList_.Get(); }
	ID3D12DescriptorHeap *GetSrvHeap() const { return srvHeap_.Get(); }
	uint32_t GetFrameIndex() const { return currentBackBufferIndex_; }

private:
	/// <summary>
	/// ディスクリプタヒープを作成する
	/// </summary>
	/// <param name="type">作成するディスクリプタヒープの種類</param>
	/// <param name="numDescriptors">ヒープ内に確保するディスクリプタ数</param>
	/// <param name="shaderVisible">GPU側から参照可能にするかどうか</param>
	/// <returns>作成された ID3D12DescriptorHeap のポインタ</returns>
	ID3D12DescriptorHeap *CreateDescriptorHeap_(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

	/// <summary>
	/// フェンス値が完了するまで CPU を待機させる
	/// </summary>
	/// <param name="frameIndex">待避対象となるバックバッファインデックス</param>
	void WaitForFrame_(UINT frameIndex);

	/// <summary>
	/// FPS を一定値（60FPS）に保つための更新処理
	/// </summary>
	void UpdateFixFPS_();

	// 初期化処理
	void InitializeFixFPS_();
	void InitializeDevice_();
	void InitializeCommand_();
	void InitializeSwapChain_();
	void InitializeDescriptorHeaps_();
	void InitializeBackBuffers_();
	void InitializeDepthBuffer_();
	void InitializeRenderTargetViews_();
	void InitializeDepthStencilView_();
	void InitializeFence_();
	void InitializeViewport_();
	void InitializeScissorRect_();
	void InitializeImGui_();

private:
	static constexpr uint32_t kBufferCount = 3;
	static constexpr int64_t  kTargetFrameMicroSec = 1'000'000 / 60;

private:
	const WinApp *winApp_ = nullptr;

	// DXGIとD3D12 core
	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
	Microsoft::WRL::ComPtr<ID3D12Device>  device_;

	// コマンド
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
	std::array<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>, kBufferCount> commandAllocators_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

	// スワップチェーンとバックバッファ
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBufferCount> backBuffers_;
	UINT currentBackBufferIndex_ = 0;

	// 深度
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;

	// ヒープ
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

	// RTVハンドル
	std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kBufferCount> rtvHandles_{};

	// インクリメントサイズ
	UINT descriptorSizeRTV_ = 0;
	UINT descriptorSizeDSV_ = 0;
	UINT descriptorSizeSRV_ = 0;

	// ビューポートとシザー
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT     scissorRect_{};

	// フェンス同期
	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	HANDLE fenceEvent_ = nullptr;
	uint64_t nextFenceValue_ = 0;
	std::array<uint64_t, kBufferCount> fenceValues_{};

	// FPS制御
	std::chrono::steady_clock::time_point fpsReference_;
};
