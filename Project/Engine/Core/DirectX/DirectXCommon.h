#pragma once
#define NOMINMAX

#include <chrono>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

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
  /// <param name="winApp"></param>
  void Initialize(HWND hwnd, int width, int height);

  void PreDraw(const float clearColor[4]);
  void PostDraw();
  void WaitForGpu();

  ID3D12Device *GetDevice() const { return device_.Get(); }
  ID3D12GraphicsCommandList *GetCommandList() const {
    return commandList_.Get();
  }
  ID3D12DescriptorHeap *GetSrvHeap() const { return srvHeap_.Get(); }

  UINT GetSrvDescriptorSize() const { return descriptorSizeSRV_; }

  UINT GetBackBufferIndex() const { return currentBackBufferIndex_; }

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap,
                                           UINT index) const;

private:
  // Initialization
  void InitializeFixFPS();
  void InitializeDevice();
  void InitializeCommand();
  void InitializeSwapChain();
  void InitializeDescriptorHeaps();
  void InitializeBackBuffers();
  void InitializeDepthBuffer();
  void InitializeRenderTargetViews();
  void InitializeDepthStencilView();
  void InitializeFence();
  void InitializeViewport();
  void InitializeScissorRect();
  void InitializeImGui();

  void WaitForFrame(UINT frameIndex);
  void UpdateFixFPS();

  ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                             UINT numDescriptors,
                                             bool shaderVisible);

private:
  HWND hwnd_;
  int width_;
  int height_;

  Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
  Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
  Microsoft::WRL::ComPtr<ID3D12Device> device_;

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[2];
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

  Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;
  static constexpr UINT kBufferCount = 2;
  Microsoft::WRL::ComPtr<ID3D12Resource> backBuffers_[kBufferCount];

  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;

  Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount]{};
  UINT descriptorSizeRTV_ = 0;
  UINT descriptorSizeDSV_ = 0;
  UINT descriptorSizeSRV_ = 0;

  Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
  HANDLE fenceEvent_ = nullptr;
  uint64_t fenceValues_[kBufferCount]{};
  uint64_t nextFenceValue_ = 0;

  D3D12_VIEWPORT viewport_{};
  D3D12_RECT scissorRect_{};

  UINT currentBackBufferIndex_ = 0;

  static constexpr long long kTargetFrameMicroSec = 16667;
  std::chrono::steady_clock::time_point fpsReference_;
};
