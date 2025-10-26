#pragma once
#include <cassert>
#include <chrono>
#include <cstdint>
#include <thread>

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <wrl.h>

class WinApp;

/// <summary>
/// DirectX12 の初期化・描画・終了処理をまとめた基盤クラス。<br/>
/// デバイス、コマンド、スワップチェーン、ImGui を一括管理する。
/// </summary>
class DirectXCommon {
public:
    // ===============================
    // 定数
    // ===============================

    // バックバッファ数(フレームリソース数)。
    static constexpr uint32_t kBufferCount = 3;

    // 目標フレーム時間(60FPS ≒ 16.666ms)。
    static constexpr int64_t kTargetFrameMicroSec = 1'000'000 / 60;

public:
    // ===============================
    // ライフサイクル
    // ===============================

    /// <summary>DirectX12 を初期化する。</summary>
    /// <param name="winApp">ウィンドウ管理クラス。</param>
    void Initialize(WinApp *winApp);

    /// <summary>DirectX12 の終了処理を行う（必要に応じて GPU 同期）。</summary>
    void Finalize() noexcept;

    // ===============================
    // 毎フレーム処理
    // ===============================

    /// <summary>
    /// フレーム開始処理。<br/>
    /// RTV/DSV 設定・クリア・ImGui NewFrame を行う。
    /// </summary>
    /// <param name="clearColor">RTV クリアカラー（RGBA）。</param>
    void PreDraw(const float clearColor[4]) noexcept;

    /// <summary>
    /// フレーム終了処理。<br/>
    /// ImGui 描画、Present、フェンス Signal、60FPS 固定のスリープ調整を行う。
    /// </summary>
    void PostDraw() noexcept;

    // ===============================
    // 画面サイズ変更
    // ===============================

    /// <summary>
    /// WM_SIZE から呼び出すリサイズ処理。<br/>
    /// バックバッファ再作成と各種再設定を行う。
    /// </summary>
    /// <param name="width">新しいクライアント幅。</param>
    /// <param name="height">新しいクライアント高さ。</param>
    void Resize(uint32_t width, uint32_t height);

    // ===============================
    // アクセサ
    // ===============================

    /// <summary>DirectX12 デバイスを取得する。</summary>
    /// <returns>ID3D12Device のポインタ。</returns>
    [[nodiscard]] ID3D12Device *GetDevice() const noexcept { return device_.Get(); }

    /// <summary>グラフィックスコマンドリストを取得する。</summary>
    /// <returns>ID3D12GraphicsCommandList のポインタ。</returns>
    [[nodiscard]] ID3D12GraphicsCommandList *GetCommandList() const noexcept { return commandList_.Get(); }

    /// <summary>SRV 用ディスクリプタヒープを取得する。</summary>
    /// <returns>ID3D12DescriptorHeap のポインタ。</returns>
    [[nodiscard]] ID3D12DescriptorHeap *GetSrvHeap() const noexcept { return srvHeap_.Get(); }

    /// <summary>現在のバックバッファに対応する RTV を取得する。</summary>
    /// <returns>CPU ディスクリプタハンドル。</returns>
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const noexcept { return rtvHandles_[currentBackBufferIndex_]; }

    /// <summary>DSV を取得する。</summary>
    /// <returns>CPU ディスクリプタハンドル。</returns>
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const noexcept { return dsvHeap_->GetCPUDescriptorHandleForHeapStart(); }

    /// <summary>現在のバックバッファインデックスを取得する。</summary>
    /// <returns>バックバッファインデックス。</returns>
    [[nodiscard]] UINT GetCurrentBackBufferIndex() const noexcept { return currentBackBufferIndex_; }

    /// <summary>現在のクライアント幅（px）。</summary>
    [[nodiscard]] uint32_t GetWidth()  const noexcept { return width_; }

    /// <summary>現在のクライアント高さ（px）。</summary>
    [[nodiscard]] uint32_t GetHeight() const noexcept { return height_; }

    /// <summary>現在のフレームインデックス（バックバッファインデックスと同じ）</summary>
    [[nodiscard]] uint32_t GetFrameIndex() const noexcept { return currentBackBufferIndex_; }


private:
    // ===============================
    // 初期化処理
    // ===============================

    /// <summary>FPS 固定のための初期化（基準時刻の記録）。</summary>
    void InitializeFixFPS() noexcept;

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
    void InitializeViewport() noexcept;

    /// <summary>シザーレクトを初期化する。</summary>
    void InitializeScissorRect() noexcept;

    /// <summary>DXC コンパイラを初期化する。</summary>
    void InitializeDXCCompiler();

    /// <summary>ImGui を初期化する。</summary>
    void InitializeImGui();

    // ===============================
    // FPS 固定
    // ===============================

    /// <summary>
    /// FPS 固定のための更新。<br/>
    /// 1/60 秒に満たない場合は 1ms スリープで調整する。
    /// </summary>
    void UpdateFixFPS() noexcept;

    // ===============================
    // ユーティリティ
    // ===============================

    /// <summary>ディスクリプタヒープを生成する。</summary>
    ID3D12DescriptorHeap *CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors, bool shaderVisible);

    /// <summary>CPU ディスクリプタハンドルを取得する。</summary>
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(ID3D12DescriptorHeap *heap, UINT index) const noexcept;

    /// <summary>GPU ディスクリプタハンドルを取得する。</summary>
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(ID3D12DescriptorHeap *heap, UINT index) const noexcept;

    /// <summary>指定インデックスのフレームリソースが未完了なら待機する。</summary>
    void WaitForFrame(UINT frameIndex);

    /// <summary>現在発行中の全コマンドをフラッシュして待機する（終了/リサイズ専用）。</summary>
    void WaitForGpu();

private:
    // ===============================
    // メンバ変数
    // ===============================

    WinApp *winApp_ = nullptr; // ウィンドウ管理クラスへの参照

    uint32_t width_ = 0;  // 現在のクライアント幅
    uint32_t height_ = 0;  // 現在のクライアント高さ

    // DXGI / Device
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    Microsoft::WRL::ComPtr<IDXGIAdapter4> adapter_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // Command
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators_[kBufferCount];
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
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount]{};
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil_;

    // View
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};

    // Fence
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t nextFenceValue_ = 0;
    uint64_t fenceValues_[kBufferCount]{};
    HANDLE fenceEvent_ = nullptr;

    // DXC (シェーダコンパイラ関連)
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

    // FPS 固定用
    std::chrono::steady_clock::time_point fpsReference_{};
};
