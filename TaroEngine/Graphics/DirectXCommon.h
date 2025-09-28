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
  // ===============================
  // 定数
  // ===============================

  /// <summary>
  /// バックバッファ数（フレームリソース数）
  /// </summary>
  static constexpr uint32_t kBufferCount = 3;

public:
  // ===============================
  // ライフサイクル
  // ===============================

  /// <summary>
  /// DirectX12 を初期化する。
  /// </summary>
  void Initialize(WinApp *winApp);

  /// <summary>
  /// DirectX12 を終了処理する（必要時のみ同期を行う）。
  /// </summary>
  void Finalize();

  // ===============================
  // 毎フレーム処理
  // ===============================

  /// <summary>
  /// フレーム開始処理。<br/>
  /// RTV/DSV 設定・クリア・ImGui NewFrame を行う。
  /// </summary>
  void PreDraw(const float clearColor[4]);

  /// <summary>
  /// フレーム終了処理。<br/>
  /// ImGui 描画、Present、フェンス Signal を行う。
  /// </summary>
  void PostDraw();

  // ===============================
  // 画面サイズ変更
  // ===============================

  /// <summary>
  /// WM_SIZE から呼び出す。<br/>
  /// バックバッファ再作成と各種再設定を行う。
  /// </summary>
  void Resize(uint32_t width, uint32_t height);

  // ===============================
  // アクセサ
  // ===============================

  /// <summary>DirectX12 デバイスを取得する。</summary>
  ID3D12Device *GetDevice() const { return device_.Get(); }

  /// <summary>グラフィックスコマンドリストを取得する。</summary>
  ID3D12GraphicsCommandList *GetCommandList() const {
    return commandList_.Get();
  }

  /// <summary>SRV 用ディスクリプタヒープを取得する。</summary>
  ID3D12DescriptorHeap *GetSrvHeap() const { return srvHeap_.Get(); }

  /// <summary>現在のバックバッファに対応する RTV を取得する。</summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const {
    return rtvHandles_[currentBackBufferIndex_];
  }

  /// <summary>DSV を取得する。</summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const {
    return dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  }

  /// <summary>現在のバックバッファインデックスを取得する。</summary>
  UINT GetCurrentBackBufferIndex() const { return currentBackBufferIndex_; }

  /// <summary>現在のクライアント幅を取得する。</summary>
  uint32_t GetWidth() const { return width_; }

  /// <summary>現在のクライアント高さを取得する。</summary>
  uint32_t GetHeight() const { return height_; }

private:
  // ===============================
  // 初期化処理
  // ===============================

  /// <summary>デバイスを初期化する。</summary>
  void InitializeDevice();
  /// <summary>コマンド関連を初期化する。</summary>
  void InitializeCommand();
  /// <summary>スワップチェーンを初期化する。</summary>
  void InitializeSwapChain();
  /// <summary>バックバッファを初期化する。</summary>
  void InitializeBackBuffers();
  /// <summary>深度バッファを初期化する。</summary>
  void InitializeDepthBuffer();
  /// <summary>ディスクリプタヒープを初期化する。</summary>
  void InitializeDescriptorHeaps();
  /// <summary>レンダーターゲットビューを初期化する。</summary>
  void InitializeRenderTargetViews();
  /// <summary>デプスステンシルビューを初期化する。</summary>
  void InitializeDepthStencilView();
  /// <summary>フェンスを初期化する。</summary>
  void InitializeFence();
  /// <summary>ビューポートを初期化する。</summary>
  void InitializeViewport();
  /// <summary>シザーレクトを初期化する。</summary>
  void InitializeScissorRect();
  /// <summary>DXC コンパイラを初期化する。</summary>
  void InitializeDXCCompiler();
  /// <summary>ImGui を初期化する。</summary>
  void InitializeImGui();

  // ===============================
  // ユーティリティ
  // ===============================

  /// <summary>
  /// ディスクリプタヒープを生成する。
  /// </summary>
  ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             UINT numDescriptors,
                                             bool shaderVisible);

  /// <summary>
  /// CPU ディスクリプタハンドルを取得する。
  /// </summary>
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

  /// <summary>
  /// GPU ディスクリプタハンドルを取得する。
  /// </summary>
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

  /// <summary>
  /// 指定インデックスのフレームリソースが完了していなければ待機する。
  /// </summary>
  void WaitForFrame(UINT frameIndex);

  /// <summary>
  /// 現在発行中の全コマンドをフラッシュして待機する。<br/>
  /// （終了時やリサイズ時専用）
  /// </summary>
  void WaitForGpu();

private:
  // ===============================
  // メンバ変数
  // ===============================

  WinApp *winApp_ = nullptr; // ウィンドウ管理クラスへの参照

  // 現在のクライアントサイズ
  uint32_t width_ = 0;
  uint32_t height_ = 0;

  // DXGI / Device
  Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  // Command
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator>
      commandAllocators_[kBufferCount];
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
  uint64_t nextFenceValue_ = 0;
  uint64_t fenceValues_[kBufferCount] = {};
  HANDLE fenceEvent_ = nullptr;

  // DXC (シェーダコンパイラ関連)
  Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
  Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
  Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};
