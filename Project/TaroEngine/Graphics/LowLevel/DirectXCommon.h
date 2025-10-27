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
#include <dxcapi.h>

class WinApp;

/// <summary>
/// DirectX12 の初期化・描画・終了処理をまとめた基盤クラス。
/// デバイス、コマンド、スワップチェーン、ImGui、フェンス同期まで面倒を見る。
/// </summary>
class DirectXCommon {
public:
    // バックバッファ数(フレームリソース数)
    static constexpr uint32_t kBufferCount = 3;

    // 60FPS目標(≒16.666ms)
    static constexpr int64_t kTargetFrameMicroSec = 1'000'000 / 60;

public:
    // ライフサイクル
    void Initialize(WinApp *winApp);
    void Finalize() noexcept;

    // 毎フレームの描画シーケンス
    void PreDraw(const float clearColor[4]) noexcept;
    void PostDraw() noexcept;

    // ウィンドウリサイズ対応
    void Resize(uint32_t width, uint32_t height);

    // -------- 追加で使いたい公開アクセサ --------
    inline ID3D12Device *GetDevice() const noexcept { return device_.Get(); }
    inline ID3D12GraphicsCommandList *GetCommandList() const noexcept { return commandList_.Get(); }
    inline ID3D12DescriptorHeap *GetSrvHeap() const noexcept { return srvHeap_.Get(); }
    inline uint32_t GetWidth() const noexcept { return width_; }
    inline uint32_t GetHeight() const noexcept { return height_; }
    /// <summary>現在のフレームインデックス（バックバッファインデックスと同じ）</summary>
    [[nodiscard]] uint32_t GetFrameIndex() const noexcept { return currentBackBufferIndex_; }

    /// <summary>
    /// GPUの実行が今積んでるところまで全部終わるまで待機する。
    /// シーン切替前に古いリソースを安全に破棄したいときに使う。
    /// </summary>
    void WaitForGpu();

    // ディスクリプタヘルパ
    ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap, UINT index) const noexcept;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap, UINT index) const noexcept;

private:
    // ========== 内部ユーティリティ ==========
    void WaitForFrame(UINT frameIndex);
    void UpdateFixFPS() noexcept;

    // 初期化サブルーチン
    void InitializeFixFPS() noexcept;
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeDescriptorHeaps();
    void InitializeBackBuffers();
    void InitializeDepthBuffer();
    void InitializeRenderTargetViews();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeViewport() noexcept;
    void InitializeScissorRect() noexcept;
    void InitializeDXCCompiler();
    void InitializeImGui();

private:
    WinApp *winApp_ = nullptr;

    // ウィンドウサイズ
    uint32_t width_ = 0;
    uint32_t height_ = 0;

    // DXGI / D3D12 core
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
    Microsoft::WRL::ComPtr<ID3D12Device>  device_;

    // コマンド周り
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

    // ビューポート / シザー
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT     scissorRect_{};

    // フェンス同期
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    HANDLE fenceEvent_ = nullptr;
    uint64_t nextFenceValue_ = 0;
    std::array<uint64_t, kBufferCount> fenceValues_{};

    // HLSL / DXC
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    // FPS制御
    std::chrono::steady_clock::time_point fpsReference_;
};
