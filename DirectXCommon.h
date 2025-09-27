#pragma once
#include <cassert>
#include <cstdint>
#include <d3d12.h>
#include <dxcapi.h>
#include <dxgi1_6.h>
#include <windows.h>
#include <wrl.h>

class WinApp;

/// <summary>
/// DirectX12 の初期化・描画・終了処理をまとめた基盤クラス。<br/>
/// デバイスやコマンド、スワップチェーン、ImGui などを一括管理する。
/// </summary>
class DirectXCommon {
public:
  // バックバッファ数
  static constexpr uint32_t kBufferCount = 3;

  /// <summary>DirectX12 初期化</summary>
  void Initialize(WinApp *winApp);

  /// <summary>DirectX12 終了（必要時のみ同期）</summary>
  void Finalize();

  /// <summary>フレーム開始：RTV/DSV 設定・クリア・ImGui NewFrame</summary>
  void PreDraw(const float clearColor[4]);

  /// <summary>フレーム終了：ImGui 描画、Present、フェンス Signal</summary>
  void PostDraw();

  // ==== アクセサ ====
  ID3D12Device *GetDevice() const { return device_.Get(); }
  ID3D12GraphicsCommandList *GetCommandList() const {
    return commandList_.Get();
  }
  ID3D12DescriptorHeap *GetSrvHeap() const { return srvHeap_.Get(); }
  D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const {
    return rtvHandles_[currentBackBufferIndex_];
  }
  D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const {
    return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  }
  UINT GetCurrentBackBufferIndex() const { return currentBackBufferIndex_; }

private:
  // ====== 初期化処理 ======
  void InitializeDevice();
  void InitializeCommand();
  void InitializeSwapChain();
  void InitializeBackBuffers();
  void InitializeDepthBuffer();
  void InitializeDescriptorHeaps();
  void InitializeRenderTargetViews();
  void InitializeDepthStencilView();
  void InitializeFence();
  void InitializeViewport();
  void InitializeScissorRect();
  void InitializeDXCCompiler();
  void InitializeImGui();

  // ====== ユーティリティ ======
  ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             UINT numDescriptors,
                                             bool shaderVisible);
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

  /// <summary>指定インデックスのフレームリソースが完了していなければ待機</summary>
  void WaitForFrame(UINT frameIndex);

  /// <summary>「今飛んでいる全仕事」を一度フラッシュして待機（終了時/リサイズ時用）</summary>
  void WaitForGpu();

private:
  // 参照
  WinApp *winApp_ = nullptr;

  // DXGI / Device
  Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  // Command
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
  // ★ フレームごとのアロケータ
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
      commandAllocators_[kBufferCount];
  // コマンドリストは 1 本（毎フレーム、該当アロケータで Reset）
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

  // SwapChain / RenderTargets
  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
  Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[kBufferCount];
  UINT currentBackBufferIndex_ = 0;

  // Descriptor Heaps
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

  UINT descriptorSizeRTV_ = 0;
  UINT descriptorSizeDSV_ = 0;
  UINT descriptorSizeSRV_ = 0;

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount] = {};
  Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;

  // View
  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissorRect_{};

  // Fence
  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  // ★ 次に使うフェンス値（単調増加）
  uint64_t nextFenceValue_ = 0;
  // ★ バックバッファごとの「発行済みフェンス値」
  uint64_t fenceValues_[kBufferCount] = {};
  HANDLE fenceEvent_ = nullptr;

  // DXC
  Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
