#include "DirectXCommon.h"
#include "WinApp.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"
#include <format>
#include <vector>

// ===== ユーティリティ =====
ID3D12DescriptorHeap *
DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type,
                                    UINT numDescriptors, bool shaderVisible) {
  D3D12_DESCRIPTOR_HEAP_DESC desc{};
  desc.Type = type;
  desc.NumDescriptors = numDescriptors;
  desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                             : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

  ID3D12DescriptorHeap *heap = nullptr;
  HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
  assert(SUCCEEDED(hr));
  return heap;
}

D3D12_CPU_DESCRIPTOR_HANDLE
DirectXCommon::GetCPUHandle(ID3D12DescriptorHeap *heap, UINT index) const {
  D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
  h.ptr += static_cast<SIZE_T>(index) *
           device_->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
  return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE
DirectXCommon::GetGPUHandle(ID3D12DescriptorHeap *heap, UINT index) const {
  D3D12_GPU_DESCRIPTOR_HANDLE h = heap->GetGPUDescriptorHandleForHeapStart();
  h.ptr += static_cast<UINT64>(index) *
           device_->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
  return h;
}

// ====== 同期 ======
void DirectXCommon::WaitForFrame(UINT frameIndex) {
  const uint64_t fenceValue = fenceValues_[frameIndex];
  if (fenceValue == 0)
    return; // まだ一度も投げていなければ何もしない
  if (fence_->GetCompletedValue() >= fenceValue)
    return; // もう終わってる

  HRESULT hr = fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
  assert(SUCCEEDED(hr));
  WaitForSingleObject(fenceEvent_, INFINITE);
}

void DirectXCommon::WaitForGpu() {
  // 「今出せる最新のフェンス値」を発行して待つ（Flush）
  const uint64_t fenceToWait = ++nextFenceValue_;
  HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceToWait);
  assert(SUCCEEDED(hr));

  if (fence_->GetCompletedValue() < fenceToWait) {
    hr = fence_->SetEventOnCompletion(fenceToWait, fenceEvent_);
    assert(SUCCEEDED(hr));
    WaitForSingleObject(fenceEvent_, INFINITE);
  }
}

// ===== Public =====
void DirectXCommon::Initialize(WinApp *winApp) {
  assert(winApp);
  winApp_ = winApp;

#ifdef _DEBUG
  Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif

  InitializeDevice();
  InitializeCommand();
  InitializeSwapChain();
  InitializeDescriptorHeaps();
  InitializeBackBuffers();
  InitializeDepthBuffer();
  InitializeRenderTargetViews();
  InitializeDepthStencilView();
  InitializeFence();
  InitializeViewport();
  InitializeScissorRect();
  InitializeDXCCompiler();
  InitializeImGui();
}

void DirectXCommon::Finalize() {
  // 必要な場面のみ同期（ここは Flush して安全に終了）
  WaitForGpu();

  // ImGui 終了
  ImGui_ImplDX12_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  if (fenceEvent_) {
    CloseHandle(fenceEvent_);
    fenceEvent_ = nullptr;
  }
}

void DirectXCommon::PreDraw(const float clearColor[4]) {
  // 今回使うバックバッファ
  currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

  // ★ このフレームのアロケータが使えるまで待つ（未完了なら）
  WaitForFrame(currentBackBufferIndex_);

  // Present -> RenderTarget
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  // ★ 今回のフレームのアロケータで Reset
  auto *allocator = commandAllocators_[currentBackBufferIndex_].Get();
  allocator->Reset();
  commandList_->Reset(allocator, nullptr);
  commandList_->ResourceBarrier(1, &barrier);

  // RTV/DSV 設定
  D3D12_CPU_DESCRIPTOR_HANDLE dsv =
      dsvHeap_->GetCPUDescriptorHandleForHeapStart();
  commandList_->OMSetRenderTargets(1, &rtvHandles_[currentBackBufferIndex_],
                                   FALSE, &dsv);

  // クリア
  commandList_->ClearRenderTargetView(rtvHandles_[currentBackBufferIndex_],
                                      clearColor, 0, nullptr);
  commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0,
                                      nullptr);

  // 描画用ヒープ
  ID3D12DescriptorHeap *heaps[] = {srvHeap_.Get()};
  commandList_->SetDescriptorHeaps(1, heaps);

  // ビューポート/シザー
  commandList_->RSSetViewports(1, &viewport_);
  commandList_->RSSetScissorRects(1, &scissorRect_);

  // ImGui フレーム開始
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

void DirectXCommon::PostDraw() {
  // RenderTarget -> Present
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

  // ImGui 描画
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

  commandList_->ResourceBarrier(1, &barrier);
  commandList_->Close();

  ID3D12CommandList *lists[] = {commandList_.Get()};
  commandQueue_->ExecuteCommandLists(1, lists);

  // ★ 今回のフレームに対するフェンス値を発行し、インデックスに記録
  const uint64_t fenceToSignal = ++nextFenceValue_;
  HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceToSignal);
  assert(SUCCEEDED(hr));
  fenceValues_[currentBackBufferIndex_] = fenceToSignal;

  // Present（垂直同期は 1。必要なら可変）
  swapChain_->Present(1, 0);

  // ★
  // ここでは待たない！（次のフレームで該当バッファを再利用するときにだけ待つ）
}

// ===== Private =====
void DirectXCommon::InitializeDevice() {
  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
  assert(SUCCEEDED(hr));

  // ハイパフォーマンスアダプタ
  for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
                       i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                       IID_PPV_ARGS(&adapter_)) != DXGI_ERROR_NOT_FOUND;
       ++i) {
    DXGI_ADAPTER_DESC3 desc{};
    adapter_->GetDesc3(&desc);
    if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
      break;
    adapter_.Reset();
  }
  assert(adapter_);

  // デバイス作成
  static D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
  for (auto lv : levels) {
    hr = D3D12CreateDevice(adapter_.Get(), lv, IID_PPV_ARGS(&device_));
    if (SUCCEEDED(hr))
      break;
  }
  assert(device_);

#ifdef _DEBUG
  Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
  }
#endif
}

void DirectXCommon::InitializeCommand() {
  HRESULT hr{};
  // Queue
  D3D12_COMMAND_QUEUE_DESC qdesc{};
  qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = device_->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&commandQueue_));
  assert(SUCCEEDED(hr));

  // ★ Allocator をバックバッファ数分
  for (UINT i = 0; i < kBufferCount; ++i) {
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&commandAllocators_[i]));
    assert(SUCCEEDED(hr));
  }

  // List は 1 本
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  commandAllocators_[0].Get(), nullptr,
                                  IID_PPV_ARGS(&commandList_));
  assert(SUCCEEDED(hr));
  commandList_->Close();
}

void DirectXCommon::InitializeSwapChain() {
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = 0;  // 自動
  desc.Height = 0; // 自動
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = kBufferCount;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> sc1;

  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(), winApp_->GetHwnd(), &desc, nullptr, nullptr,
      sc1.GetAddressOf());
  assert(SUCCEEDED(hr));

  // IDXGISwapChain4 へ昇格
  hr = sc1.As(&swapChain_);
  assert(SUCCEEDED(hr));

  // Alt+Enter を無効化
  dxgiFactory_->MakeWindowAssociation(winApp_->GetHwnd(),
                                      DXGI_MWA_NO_ALT_ENTER);
}

void DirectXCommon::InitializeDescriptorHeaps() {
  descriptorSizeRTV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  descriptorSizeDSV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  rtvHeap_.Attach(CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                       kBufferCount, false));
  dsvHeap_.Attach(
      CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false));
  srvHeap_.Attach(
      CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true));
}

void DirectXCommon::InitializeBackBuffers() {
  for (UINT i = 0; i < kBufferCount; ++i) {
    HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
    assert(SUCCEEDED(hr));
  }
}

void DirectXCommon::InitializeDepthBuffer() {
  D3D12_RESOURCE_DESC res{};
  res.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  res.Width = static_cast<UINT>(winApp_->kClientWidth);
  res.Height = static_cast<UINT>(winApp_->kClientHeight);
  res.DepthOrArraySize = 1;
  res.MipLevels = 1;
  res.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  res.SampleDesc.Count = 1;
  res.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

  D3D12_HEAP_PROPERTIES heap{};
  heap.Type = D3D12_HEAP_TYPE_DEFAULT;

  D3D12_CLEAR_VALUE clear{};
  clear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  clear.DepthStencil.Depth = 1.0f;
  clear.DepthStencil.Stencil = 0;

  HRESULT hr = device_->CreateCommittedResource(
      &heap, D3D12_HEAP_FLAG_NONE, &res, D3D12_RESOURCE_STATE_DEPTH_WRITE,
      &clear, IID_PPV_ARGS(&depthStencil_));
  assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeRenderTargetViews() {
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  auto base = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
  for (UINT i = 0; i < kBufferCount; ++i) {
    rtvHandles_[i] = base;
    rtvHandles_[i].ptr += static_cast<SIZE_T>(i) * descriptorSizeRTV_;
    device_->CreateRenderTargetView(backBuffers_[i].Get(), &rtvDesc,
                                    rtvHandles_[i]);
  }
}

void DirectXCommon::InitializeDepthStencilView() {
  D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
  desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
  desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

  device_->CreateDepthStencilView(
      depthStencil_.Get(), &desc,
      dsvHeap_->GetCPUDescriptorHandleForHeapStart());
}

void DirectXCommon::InitializeFence() {
  HRESULT hr =
      device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
  assert(SUCCEEDED(hr));

  fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  assert(fenceEvent_ != nullptr);

  nextFenceValue_ = 0;
  for (UINT i = 0; i < kBufferCount; ++i)
    fenceValues_[i] = 0;
}

void DirectXCommon::InitializeViewport() {
  viewport_.Width = static_cast<float>(winApp_->kClientWidth);
  viewport_.Height = static_cast<float>(winApp_->kClientHeight);
  viewport_.TopLeftX = 0.0f;
  viewport_.TopLeftY = 0.0f;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect() {
  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = static_cast<LONG>(winApp_->kClientWidth);
  scissorRect_.bottom = static_cast<LONG>(winApp_->kClientHeight);
}

void DirectXCommon::InitializeDXCCompiler() {
  HRESULT hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
  assert(SUCCEEDED(hr));
  hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
  assert(SUCCEEDED(hr));
  hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
  assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeImGui() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplWin32_Init(winApp_->GetHwnd());
  ImGui_ImplDX12_Init(device_.Get(), kBufferCount,
                      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap_.Get(),
                      srvHeap_->GetCPUDescriptorHandleForHeapStart(),
                      srvHeap_->GetGPUDescriptorHandleForHeapStart());
}
