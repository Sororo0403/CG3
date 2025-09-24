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
  static constexpr uint32_t kBufferCount = 2;

  /// <summary>
  /// DirectX12 初期化処理。WinApp への依存あり。
  /// </summary>
  /// <param name="winApp">ウィンドウ管理クラスへのポインタ。</param>
  void Initialize(WinApp *winApp);

  /// <summary>
  /// DirectX12 の終了処理。フェンス待機や ImGui のシャットダウンも含む。
  /// </summary>
  void Finalize();

  /// <summary>
  /// フレーム開始処理。バックバッファ遷移、RTV/DSV 設定、クリア、ImGui
  /// フレーム開始など。
  /// </summary>
  /// <param name="clearColor">画面クリアカラー (RGBA)。</param>
  void PreDraw(const float clearColor[4]);

  /// <summary>
  /// フレーム終了処理。ImGui 描画、リソース遷移、Present、フェンス同期など。
  /// </summary>
  void PostDraw();

  // ==== アクセサ ====

  /// <summary>
  /// DirectX12 デバイスを取得。
  /// </summary>
  ID3D12Device *GetDevice() const { return device_.Get(); }

  /// <summary>
  /// コマンドリストを取得。
  /// </summary>
  ID3D12GraphicsCommandList *GetCommandList() const {
    return commandList_.Get();
  }

  /// <summary>
  /// SRV用ディスクリプタヒープを取得。
  /// </summary>
  ID3D12DescriptorHeap *GetSrvHeap() const { return srvHeap_.Get(); }

  /// <summary>
  /// 現在のバックバッファの RTV ハンドルを取得。
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const {
    return rtvHandles_[currentBackBufferIndex_];
  }

  /// <summary>
  /// 深度ステンシルビュー (DSV) のハンドルを取得。
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const {
    return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  }

  /// <summary>
  /// 現在のバックバッファインデックスを取得。
  /// </summary>
  UINT GetCurrentBackBufferIndex() const { return currentBackBufferIndex_; }

private:
  // ====== 初期化処理 ======
  /// <summary>
  /// DXGIファクトリとデバイスの生成。
  /// </summary>
  void InitializeDevice();

  /// <summary>
  /// コマンドキュー、アロケータ、リストの生成。
  /// </summary>
  void InitializeCommand();

  /// 
  /// <summary>スワップチェーン生成。
  /// </summary>
  void InitializeSwapChain();

  /// <summary>
  /// バックバッファの取得。
  /// </summary>
  void InitializeBackBuffers();

  /// <summary>
  /// 深度バッファ生成。
  /// </summary>
  void InitializeDepthBuffer();

  /// <summary>
  /// 各ディスクリプタヒープ生成 (RTV/DSV/SRV)。
  /// </summary>
  void InitializeDescriptorHeaps();

  /// <summary>
  /// RTV の作成。
  /// </summary>
  void InitializeRenderTargetViews();

  /// <summary>
  /// DSV の作成。
  /// </summary>
  void InitializeDepthStencilView();

  /// <summary>
  /// フェンスとイベントの初期化。
  /// </summary>
  void InitializeFence();

  /// <summary>
  /// ビューポート設定。
  /// </summary>
  void InitializeViewport();

  /// <summary>
  /// シザー矩形設定。
  /// </summary>
  void InitializeScissorRect();

  /// <summary>
  /// DXC (HLSLコンパイラ) の初期化。
  /// </summary>
  void InitializeDXCCompiler();

  /// <summary>
  /// ImGui の初期化。
  /// </summary>
  void InitializeImGui();

  // ====== ユーティリティ ======
  /// <summary>
  /// ディスクリプタヒープを生成。
  /// </summary>
  ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             UINT numDescriptors,
                                             bool shaderVisible);

  /// <summary>
  /// CPU ディスクリプタハンドルを計算して返す。
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

  /// <summary>
  /// GPU ディスクリプタハンドルを計算して返す。
  /// </summary>
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

  /// <summary>
  /// GPU の処理完了をフェンスで待機する。
  /// </summary>
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
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
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
  uint64_t fenceValue_ = 0;
  HANDLE fenceEvent_ = nullptr;

  // DXC
  Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
