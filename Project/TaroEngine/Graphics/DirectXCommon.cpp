#define NOMINMAX
#include "DirectXCommon.h"
#include "WinApp.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
#include <algorithm>
#include <format>
#include <vector>

using Microsoft::WRL::ComPtr;

// =====================================
// ユーティリティ
// =====================================

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

// =====================================
// 同期
// =====================================

void DirectXCommon::WaitForFrame(UINT frameIndex) {
  const uint64_t fenceValue = fenceValues_[frameIndex];
  if (fenceValue == 0)
    return; // まだ Signal していない
  if (fence_->GetCompletedValue() >= fenceValue)
    return; // 既に完了

  HRESULT hr = fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
  assert(SUCCEEDED(hr));
  WaitForSingleObject(fenceEvent_, INFINITE);
}

void DirectXCommon::WaitForGpu() {
  // 現在のキューにフェンスを打って待機（全体フラッシュ）
  const uint64_t fenceToWait = ++nextFenceValue_;
  HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceToWait);
  assert(SUCCEEDED(hr));

  if (fence_->GetCompletedValue() < fenceToWait) {
    hr = fence_->SetEventOnCompletion(fenceToWait, fenceEvent_);
    assert(SUCCEEDED(hr));
    WaitForSingleObject(fenceEvent_, INFINITE);
  }
}

// =====================================
// Public
// =====================================

void DirectXCommon::Initialize(WinApp *winApp) {
  assert(winApp);
  winApp_ = winApp;

  // 実ウィンドウのクライアントサイズ取得（最小値 1）
  RECT rc{};
  GetClientRect(winApp_->GetHwnd(), &rc);
  width_ = std::max(1L, rc.right - rc.left);
  height_ = std::max(1L, rc.bottom - rc.top);

#ifdef _DEBUG
  // デバッグレイヤー有効化（GPU Based Validation もオン）
  ComPtr<ID3D12Debug1> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    debugController->SetEnableGPUBasedValidation(TRUE);
  }
#endif

  // 初期化シーケンス
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
  // 終了前にフラッシュ（未完了の仕事を待つ）
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
  // 最小化中は何もしない
  if (width_ == 0 || height_ == 0)
    return;

  // 今回使うバックバッファ
  currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

  // このフレームに対応するアロケータが空くまで（必要なら）待機
  WaitForFrame(currentBackBufferIndex_);

  // Present -> RenderTarget 遷移
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

  // 今回のアロケータでリセット
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

  // 描画用ディスクリプタヒープ設定（SRV など）
  ID3D12DescriptorHeap *heaps[] = {srvHeap_.Get()};
  commandList_->SetDescriptorHeaps(1, heaps);

  // VP/Scissor
  commandList_->RSSetViewports(1, &viewport_);
  commandList_->RSSetScissorRects(1, &scissorRect_);

  // ImGui フレーム開始
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
}

void DirectXCommon::PostDraw() {
  if (width_ == 0 || height_ == 0)
    return;

  // ImGui を描画コマンドへ発行
  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

  // RenderTarget -> Present 遷移
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = backBuffers_[currentBackBufferIndex_].Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
  commandList_->ResourceBarrier(1, &barrier);

  // コマンドリストを閉じて実行
  commandList_->Close();
  ID3D12CommandList *lists[] = {commandList_.Get()};
  commandQueue_->ExecuteCommandLists(1, lists);

  // 今フレーム用のフェンス値を発行して記録
  const uint64_t fenceToSignal = ++nextFenceValue_;
  HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceToSignal);
  assert(SUCCEEDED(hr));
  fenceValues_[currentBackBufferIndex_] = fenceToSignal;

  // Present（必要に応じて 0 / tearing に切り替え可）
  swapChain_->Present(1, 0);

  // ここでは待たない（次回使用時に WaitForFrame で同期）
}

void DirectXCommon::Resize(uint32_t width, uint32_t height) {
  // 0x0（最小化）や同一サイズは処理不要
  if (width == 0 || height == 0)
    return;
  if (width_ == width && height_ == height)
    return;

  // 安全のため一度フラッシュ
  WaitForGpu();

  // 古いバックバッファ類を解放
  for (UINT i = 0; i < kBufferCount; ++i)
    backBuffers_[i].Reset();
  depthStencil_.Reset();

  // スワップチェーンのリサイズ
  HRESULT hr = swapChain_->ResizeBuffers(
      kBufferCount, width, height,
      DXGI_FORMAT_R8G8B8A8_UNORM, // 既存設定に合わせる
      0 /* 将来 tearing を使うなら DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING */);
  assert(SUCCEEDED(hr));

  // 新しいインデックス＆サイズを反映
  currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();
  width_ = width;
  height_ = height;

  // 依存リソースを再構築
  InitializeBackBuffers();
  InitializeRenderTargetViews();
  InitializeDepthBuffer();
  InitializeDepthStencilView();
  InitializeViewport();
  InitializeScissorRect();
}

// =====================================
// Private 初期化群
// =====================================

void DirectXCommon::InitializeDevice() {
  // DXGI ファクトリ作成
  HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
  assert(SUCCEEDED(hr));

  // ハイパフォーマンス GPU を優先して選択（ソフトウェアは除外）
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

  // D3D12 デバイス作成（高い FL から順にトライ）
  static D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0};
  for (auto lv : levels) {
    hr = D3D12CreateDevice(adapter_.Get(), lv, IID_PPV_ARGS(&device_));
    if (SUCCEEDED(hr))
      break;
  }
  assert(device_);

#ifdef _DEBUG
  // 重大メッセージでブレーク
  ComPtr<ID3D12InfoQueue> infoQueue;
  if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
  }
#endif
}

void DirectXCommon::InitializeCommand() {
  HRESULT hr{};

  // コマンドキュー
  D3D12_COMMAND_QUEUE_DESC qdesc{};
  qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  hr = device_->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&commandQueue_));
  assert(SUCCEEDED(hr));

  // フレーム数分のアロケータ
  for (UINT i = 0; i < kBufferCount; ++i) {
    hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                         IID_PPV_ARGS(&commandAllocators_[i]));
    assert(SUCCEEDED(hr));
  }

  // コマンドリストは 1 本（毎フレーム Reset）
  hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  commandAllocators_[0].Get(), nullptr,
                                  IID_PPV_ARGS(&commandList_));
  assert(SUCCEEDED(hr));
  commandList_->Close();
}

void DirectXCommon::InitializeSwapChain() {
  // スワップチェーン基本設定（Flip Model）
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = 0;  // 自動
  desc.Height = 0; // 自動
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = kBufferCount;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  // desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING; // 必要に応じて

  ComPtr<IDXGISwapChain1> sc1;
  HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
      commandQueue_.Get(), winApp_->GetHwnd(), &desc, nullptr, nullptr,
      sc1.GetAddressOf());
  assert(SUCCEEDED(hr));

  // IDXGISwapChain4 に昇格
  hr = sc1.As(&swapChain_);
  assert(SUCCEEDED(hr));

  // Alt+Enter のフルスクリーン切替を無効化（アプリ側で制御するため）
  dxgiFactory_->MakeWindowAssociation(winApp_->GetHwnd(),
                                      DXGI_MWA_NO_ALT_ENTER);
}

void DirectXCommon::InitializeDescriptorHeaps() {
  // 増分サイズ取得
  descriptorSizeRTV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  descriptorSizeDSV_ =
      device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
  descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

  // 必要なヒープ作成
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
  // D24S8 の 2D テクスチャを作成
  D3D12_RESOURCE_DESC res{};
  res.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  res.Width = static_cast<UINT>(width_);
  res.Height = static_cast<UINT>(height_);
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
  // SRGB 書き込み（ガンマ補正）にする場合は UNORM_SRGB を選択
  D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
  rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
  rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

  // 連番で RTV を配置
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
  viewport_.Width = static_cast<float>(width_);
  viewport_.Height = static_cast<float>(height_);
  viewport_.TopLeftX = 0.0f;
  viewport_.TopLeftY = 0.0f;
  viewport_.MinDepth = 0.0f;
  viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect() {
  scissorRect_.left = 0;
  scissorRect_.top = 0;
  scissorRect_.right = static_cast<LONG>(width_);
  scissorRect_.bottom = static_cast<LONG>(height_);
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
  // ImGui 基本セットアップ
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  // Win32/DX12 バックエンド初期化
  ImGui_ImplWin32_Init(winApp_->GetHwnd());
  ImGui_ImplDX12_Init(device_.Get(), kBufferCount,
                      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap_.Get(),
                      srvHeap_->GetCPUDescriptorHandleForHeapStart(),
                      srvHeap_->GetGPUDescriptorHandleForHeapStart());
}
