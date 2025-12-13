#define NOMINMAX

#include "DirectXCommon.h"

#include <cassert>

#include <d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"

#include "Logger/Logger.h"

using Microsoft::WRL::ComPtr;

DirectXCommon::DirectXCommon(HWND hwnd, int width, int height) {
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
}

DirectXCommon::~DirectXCommon() {
    LOG_INFO("DirectXCommon destructor begin");

    WaitForGpu();

    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }

    LOG_INFO("DirectXCommon destructor end");
}

void DirectXCommon::Initialize() {
    LOG_INFO("DirectXCommon::Initialize start");

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
    InitializeImGui();

    LOG_INFO("DirectXCommon::Initialize completed");
}

void DirectXCommon::PreDraw(const float clearColor[4]) {
    currentBackBufferIndex_ = swapChain_->GetCurrentBackBufferIndex();

    WaitForFrame(currentBackBufferIndex_);

    auto *allocator = commandAllocators_[currentBackBufferIndex_].Get();
    allocator->Reset();
    commandList_->Reset(allocator, nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffers_[currentBackBufferIndex_].Get(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList_->ResourceBarrier(1, &barrier);

    auto rtv = rtvHandles_[currentBackBufferIndex_];
    auto dsv = dsvHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    commandList_->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    commandList_->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0,
                                        nullptr);

    ID3D12DescriptorHeap *heaps[] = {srvHeap_.Get()};
    commandList_->SetDescriptorHeaps(1, heaps);

    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);

    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void DirectXCommon::PostDraw() {
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList_.Get());

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        backBuffers_[currentBackBufferIndex_].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    commandList_->ResourceBarrier(1, &barrier);

    commandList_->Close();

    ID3D12CommandList *lists[] = {commandList_.Get()};
    commandQueue_->ExecuteCommandLists(1, lists);

    uint64_t fv = ++nextFenceValue_;
    HRESULT hr = commandQueue_->Signal(fence_.Get(), fv);
    if (FAILED(hr)) {
        LOG_ERROR("PostDraw: commandQueue->Signal failed");
        assert(false);
    }
    fenceValues_[currentBackBufferIndex_] = fv;

    hr = swapChain_->Present(1, 0);
    if (FAILED(hr)) {
        LOG_ERROR("PostDraw: swapChain->Present failed");
        assert(false);
    }
}

void DirectXCommon::WaitForGpu() {
    if (!commandQueue_ || !fence_)
        return;

    uint64_t fenceToWait = ++nextFenceValue_;
    HRESULT hr = commandQueue_->Signal(fence_.Get(), fenceToWait);
    if (FAILED(hr)) {
        LOG_ERROR("WaitForGpu: commandQueue->Signal failed");
        assert(false);
    }

    if (fence_->GetCompletedValue() < fenceToWait) {
        hr = fence_->SetEventOnCompletion(fenceToWait, fenceEvent_);
        if (FAILED(hr)) {
            LOG_ERROR("WaitForGpu: SetEventOnCompletion failed");
            assert(false);
        }
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void DirectXCommon::InitializeDevice() {
    LOG_INFO("Creating DXGI Factory");

    HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
    if (FAILED(hr)) {
        LOG_ERROR("CreateDXGIFactory failed");
        assert(false);
    }

    LOG_INFO("Selecting high performance GPU");

    for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(
                         i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                         IID_PPV_ARGS(&adapter_)) != DXGI_ERROR_NOT_FOUND;
         ++i) {

        DXGI_ADAPTER_DESC3 desc{};
        adapter_->GetDesc3(&desc);

        if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
            LOG_INFO("GPU selected");
            break;
        }
        adapter_.Reset();
    }

    if (!adapter_) {
        LOG_ERROR("No suitable GPU adapter found");
        assert(false);
    }

    static D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };

    bool created = false;
    for (auto lv : levels) {
        hr = D3D12CreateDevice(adapter_.Get(), lv, IID_PPV_ARGS(&device_));
        if (SUCCEEDED(hr)) {
            LOG_INFO("D3D12 device created");
            created = true;
            break;
        }
    }

    if (!created) {
        LOG_ERROR("D3D12CreateDevice failed");
        assert(false);
    }
}

void DirectXCommon::InitializeCommand() {
    HRESULT hr{};

    D3D12_COMMAND_QUEUE_DESC qdesc{};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = device_->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&commandQueue_));
    if (FAILED(hr)) {
        LOG_ERROR("CreateCommandQueue failed");
        assert(false);
    }

    for (UINT i = 0; i < kBufferCount; i++) {
        hr = device_->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&commandAllocators_[i]));
        if (FAILED(hr)) {
            LOG_ERROR("CreateCommandAllocator failed index=" +
                      std::to_string(i));
            assert(false);
        }
    }

    hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                    commandAllocators_[0].Get(), nullptr,
                                    IID_PPV_ARGS(&commandList_));
    if (FAILED(hr)) {
        LOG_ERROR("CreateCommandList failed");
        assert(false);
    }

    commandList_->Close();
}

void DirectXCommon::InitializeSwapChain() {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = {1, 0};
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = kBufferCount;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd(
        commandQueue_.Get(), hwnd_, &desc, nullptr, nullptr,
        sc1.GetAddressOf());

    if (FAILED(hr)) {
        LOG_ERROR("CreateSwapChainForHwnd failed");
        assert(false);
    }

    hr = sc1.As(&swapChain_);
    if (FAILED(hr)) {
        LOG_ERROR("SwapChain1::As failed");
        assert(false);
    }

    dxgiFactory_->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER);

    LOG_INFO("SwapChain created");
}

void DirectXCommon::InitializeDescriptorHeaps() {
    descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    descriptorSizeSRV_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    rtvHeap_.Attach(CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                         kBufferCount, false));
    dsvHeap_.Attach(
        CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false));
    srvHeap_.Attach(CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                         128, true));

    LOG_INFO("Descriptor heaps created");
}

void DirectXCommon::InitializeBackBuffers() {
    for (UINT i = 0; i < kBufferCount; i++) {
        HRESULT hr = swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        if (FAILED(hr)) {
            LOG_ERROR("GetBuffer failed index=" + std::to_string(i));
            assert(false);
        }
    }

    LOG_INFO("Back buffers acquired");
}

void DirectXCommon::InitializeDepthBuffer() {
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);

    CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_D24_UNORM_S8_UINT, width_, height_, 1, 1, 1, 0,
        D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    CD3DX12_CLEAR_VALUE clear(DXGI_FORMAT_D24_UNORM_S8_UINT, 1.0f, 0);

    HRESULT hr = device_->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear, IID_PPV_ARGS(&depthStencil_));

    if (FAILED(hr)) {
        LOG_ERROR("CreateCommittedResource for depth buffer failed");
        assert(false);
    }

    LOG_INFO("Depth buffer created");
}

void DirectXCommon::InitializeRenderTargetViews() {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    auto base = rtvHeap_->GetCPUDescriptorHandleForHeapStart();

    for (UINT i = 0; i < kBufferCount; i++) {
        rtvHandles_[i] = base;
        rtvHandles_[i].ptr += static_cast<SIZE_T>(i) * descriptorSizeRTV_;

        device_->CreateRenderTargetView(backBuffers_[i].Get(), &rtvDesc,
                                        rtvHandles_[i]);
    }

    LOG_INFO("RTVs created");
}

void DirectXCommon::InitializeDepthStencilView() {
    D3D12_DEPTH_STENCIL_VIEW_DESC desc{};
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device_->CreateDepthStencilView(
        depthStencil_.Get(), &desc,
        dsvHeap_->GetCPUDescriptorHandleForHeapStart());

    LOG_INFO("DSV created");
}

void DirectXCommon::InitializeFence() {
    HRESULT hr =
        device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    if (FAILED(hr)) {
        LOG_ERROR("CreateFence failed");
        assert(false);
    }

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        LOG_ERROR("CreateEvent for fenceEvent_ failed");
        assert(false);
    }

    nextFenceValue_ = 0;
    for (auto &v : fenceValues_) {
        v = 0;
    }

    LOG_INFO("Fence initialized");
}

void DirectXCommon::InitializeViewport() {
    viewport_.TopLeftX = 0;
    viewport_.TopLeftY = 0;
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect() {
    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = width_;
    scissorRect_.bottom = height_;
}

void DirectXCommon::InitializeImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX12_Init(device_.Get(), kBufferCount,
                        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap_.Get(),
                        srvHeap_->GetCPUDescriptorHandleForHeapStart(),
                        srvHeap_->GetGPUDescriptorHandleForHeapStart());

    LOG_INFO("ImGui initialized");
}

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
    if (FAILED(hr)) {
        LOG_ERROR("CreateDescriptorHeap failed type=" +
                  std::to_string((int) type));
        assert(false);
    }

    return heap;
}

D3D12_CPU_DESCRIPTOR_HANDLE
DirectXCommon::GetCPUHandle(ID3D12DescriptorHeap *heap, UINT index) const {
    auto h = heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr +=
        index * device_->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
    return h;
}

D3D12_GPU_DESCRIPTOR_HANDLE
DirectXCommon::GetGPUHandle(ID3D12DescriptorHeap *heap, UINT index) const {
    auto h = heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr +=
        index * device_->GetDescriptorHandleIncrementSize(heap->GetDesc().Type);
    return h;
}

void DirectXCommon::WaitForFrame(UINT frameIndex) {
    uint64_t fenceValue = fenceValues_[frameIndex];

    if (fenceValue == 0)
        return;

    if (fence_->GetCompletedValue() >= fenceValue)
        return;

    fence_->SetEventOnCompletion(fenceValue, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);
}
